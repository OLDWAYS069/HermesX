#!/usr/bin/env python3
"""
Meshtastic 自動刷寫與設定工具
以 flash_and_config.ps1 的流程為基準實作 Python 版本。
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import logging
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Optional

import serial.tools.list_ports


LOGGER = logging.getLogger("meshtastic_auto_flash")
INTERNAL_HELPER_FLAG = "--internal-cli"
LOG_LINE_DELAY_SECONDS = 0.5


def normalize_exit_code(code: object) -> int:
    if code is None:
        return 0
    if isinstance(code, bool):
        return int(code)
    if isinstance(code, int):
        return code
    return 1


def run_meshtastic_entrypoint(args: list[str]) -> int:
    from meshtastic.__main__ import main as meshtastic_main

    original_argv = sys.argv[:]
    try:
        sys.argv = ["meshtastic", *args]
        return normalize_exit_code(meshtastic_main())
    except SystemExit as exc:
        return normalize_exit_code(exc.code)
    finally:
        sys.argv = original_argv


def run_esptool_entrypoint(args: list[str]) -> int:
    import esptool

    original_argv = sys.argv[:]
    try:
        sys.argv = ["esptool", *args]
        esptool._main()
        return 0
    except SystemExit as exc:
        return normalize_exit_code(exc.code)
    finally:
        sys.argv = original_argv


def try_run_internal_helper(argv: list[str]) -> Optional[int]:
    if not argv or argv[0] != INTERNAL_HELPER_FLAG:
        return None
    if len(argv) < 2:
        print("Missing internal tool name.", file=sys.stderr)
        return 2

    tool_name = argv[1]
    tool_args = argv[2:]

    if tool_name == "meshtastic":
        return run_meshtastic_entrypoint(tool_args)
    if tool_name == "esptool":
        return run_esptool_entrypoint(tool_args)

    print(f"Unknown internal tool: {tool_name}", file=sys.stderr)
    return 2


def pause_before_exit(message: str) -> None:
    print(message, flush=True)
    if not sys.stdin or not sys.stdin.isatty():
        return
    try:
        if os.name == "nt":
            import msvcrt

            msvcrt.getwch()
            return
    except Exception:
        pass
    try:
        input()
    except EOFError:
        pass


def notify_user_attention(message: str) -> None:
    print("\a", end="", flush=True)
    print(message, flush=True)


@dataclass
class MeshtasticCommand:
    type: str
    raw: str
    field: Optional[str] = None
    value: Optional[str] = None
    message: Optional[str] = None
    url: Optional[str] = None
    index: Optional[int] = None


class MeshtasticAutoFlash:
    def __init__(self) -> None:
        self.script_dir = self.get_runtime_dir()
        self.repo_root = self._find_repo_root(self.script_dir)
        self.args = self._parse_arguments()
        self.log_path = self._initialize_log()

    def _parse_arguments(self) -> argparse.Namespace:
        parser = argparse.ArgumentParser(description="Meshtastic 自動刷寫和設定工具")
        parser.add_argument("--firmware-path", default="", help="韌體檔案路徑")
        parser.add_argument("--firmware-file-name", default="HermesX_0.2.8-beta0002-update.bin", help="韌體檔案名稱")
        parser.add_argument("--config-path", default="", help="設定檔路徑，支援 YAML 或舊版 CLI.md")
        parser.add_argument("--config-file-name", default="config.yaml", help="設定檔檔名")
        parser.add_argument("--cli-config-path", default="", help="舊版 CLI 設定檔路徑")
        parser.add_argument("--cli-config-file-name", default="CLI.md", help="舊版 CLI 設定檔檔名")
        parser.add_argument("--export-config-yaml", default="", help="將現有 CLI.md 轉成 YAML 並輸出到指定路徑後結束")
        parser.add_argument("--post-flash-wait-seconds", type=int, default=60, help="刷寫後等待秒數")
        parser.add_argument("--reboot-batch-size", type=int, default=2, help="重啟批次大小")
        parser.add_argument("--reboot-wait-seconds", type=int, default=10, help="重啟等待秒數")
        parser.add_argument("--reapply-max-passes", type=int, default=2, help="重新套用最大輪數")
        parser.add_argument("--port-detect-timeout-seconds", type=int, default=60, help="序列埠檢測逾時秒數")
        parser.add_argument("--port-detect-interval-seconds", type=int, default=2, help="序列埠檢測間隔秒數")
        parser.add_argument("--ready-timeout-seconds", type=int, default=30, help="裝置就緒逾時秒數")
        parser.add_argument("--ready-poll-seconds", type=int, default=2, help="裝置就緒輪詢秒數")
        parser.add_argument("--ready-command-timeout-seconds", type=int, default=10, help="就緒檢查命令逾時秒數")
        parser.add_argument("--ready-retry-count", type=int, default=2, help="就緒檢查重試次數")
        parser.add_argument("--meshtastic-timeout-seconds", type=int, default=120, help="meshtastic CLI 逾時秒數")
        parser.add_argument("--meshtastic-retry-count", type=int, default=3, help="meshtastic CLI 重試次數")
        parser.add_argument("--meshtastic-retry-delay-seconds", type=int, default=0, help="meshtastic CLI 重試延遲秒數")
        parser.add_argument("--log-path", default="", help="日誌檔路徑")
        parser.add_argument("--reboot-after-config", action="store_true", default=True, help="設定後重啟")
        parser.add_argument("--no-reboot-after-config", action="store_false", dest="reboot_after_config", help="設定後不重啟")
        parser.add_argument("--post-config-reboot-wait-seconds", type=int, default=10, help="設定後重啟等待秒數")
        parser.add_argument("--use-transaction", action="store_true", default=False, help="使用 begin/commit edit")
        return parser.parse_args()

    def _initialize_log(self) -> Path:
        default_log_dir = self.script_dir
        log_path = Path(self.args.log_path).expanduser() if self.args.log_path else default_log_dir / "flash_and_config.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(f"==== {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} ====\n", encoding="utf-8")
        LOGGER.setLevel(logging.INFO)
        LOGGER.handlers.clear()
        formatter = logging.Formatter("[%(asctime)s] %(message)s", datefmt="%H:%M:%S")
        console = logging.StreamHandler(sys.stdout)
        console.setFormatter(formatter)
        file_handler = logging.FileHandler(log_path, encoding="utf-8")
        file_handler.setFormatter(formatter)
        LOGGER.addHandler(console)
        LOGGER.addHandler(file_handler)
        return log_path

    def log(self, message: str, delay_after: bool = True) -> None:
        LOGGER.info(message)
        if delay_after and sys.stdout.isatty():
            time.sleep(LOG_LINE_DELAY_SECONDS)

    @staticmethod
    def print_text_with_duration(text: str, duration_seconds: float) -> None:
        if not text:
            return
        if duration_seconds <= 0 or not sys.stdout.isatty():
            print(text, end="" if text.endswith("\n") else "\n", flush=True)
            return

        delay = duration_seconds / len(text)
        for char in text:
            print(char, end="", flush=True)
            time.sleep(delay)
        if not text.endswith("\n"):
            print("", flush=True)

    def print_startup_banner(self) -> None:
        banner_path = self.repo_root / "ascii-art-text-1773857730689.txt"
        if not banner_path.exists():
            return
        try:
            self.print_text_with_duration(self.read_text_file_best_encoding(banner_path), 5.0)
        except Exception as exc:
            self.log(f"讀取開場 ASCII 圖失敗：{exc}", delay_after=False)

    @staticmethod
    def get_runtime_dir() -> Path:
        if getattr(sys, "frozen", False):
            executable_dir = Path(sys.executable).resolve().parent
            if executable_dir.name in {"tool_windows", "tool_macos"}:
                return executable_dir.parent
            return executable_dir
        source_dir = Path(__file__).resolve().parent
        repo_root = MeshtasticAutoFlash._find_repo_root(source_dir)
        runtime_dir = repo_root / "auto_flash_tool"
        if runtime_dir.exists():
            return runtime_dir
        return source_dir

    @staticmethod
    def _find_repo_root(start: Path) -> Path:
        current = start
        while True:
            if (current / ".git").exists():
                return current
            if current.parent == current:
                return start
            current = current.parent

    @staticmethod
    def read_text_file_best_encoding(path: Path) -> str:
        raw = path.read_bytes()
        if raw.startswith(b"\xef\xbb\xbf"):
            return raw[3:].decode("utf-8")
        try:
            return raw.decode("utf-8")
        except UnicodeDecodeError:
            return raw.decode("big5")

    def resolve_firmware_path(self, preferred_file_name: str = "") -> Path:
        if self.args.firmware_path:
            candidate = Path(self.args.firmware_path).expanduser()
            if candidate.exists():
                return candidate.resolve()
            raise FileNotFoundError(f"Firmware not found: {candidate}")

        target_dir = self.script_dir / "Target"
        if not target_dir.exists():
            raise FileNotFoundError(f"Target 資料夾不存在: {target_dir}")

        matches = sorted(target_dir.glob("*.bin"), key=lambda item: item.stat().st_mtime, reverse=True)
        if not matches:
            raise FileNotFoundError(f"Target 資料夾找不到任何 .bin: {target_dir}")

        preferred_name = preferred_file_name.strip()
        if preferred_name:
            preferred_matches = [item for item in matches if item.name == preferred_name]
            if preferred_matches:
                self.log(f"設定檔指定韌體：{preferred_name}，將直接使用這個檔案。")
                return preferred_matches[0].resolve()

        if len(matches) == 1:
            return matches[0].resolve()

        self.log("Target 資料夾找到多個韌體檔案，請選擇：")
        for index, item in enumerate(matches, start=1):
            self.log(f"[{index}] {item.name}")

        while True:
            choice = input(f"請選擇韌體檔案 (1-{len(matches)}): ").strip()
            if choice.isdigit():
                selected_index = int(choice)
                if 1 <= selected_index <= len(matches):
                    return matches[selected_index - 1].resolve()
            print("輸入無效，請重新選擇。")

    def resolve_cli_config_path(self) -> Path:
        if self.args.cli_config_path:
            candidate = Path(self.args.cli_config_path).expanduser()
            if candidate.exists():
                return candidate.resolve()
            raise FileNotFoundError(f"找不到 CLI 設定檔：{candidate}")
        candidates = [
            self.script_dir / self.args.cli_config_file_name,
            self.repo_root / self.args.cli_config_file_name,
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate.resolve()
        raise FileNotFoundError(f"找不到 CLI 設定檔，預期檔名為：{self.args.cli_config_file_name}")

    def resolve_config_path(self) -> Path:
        if self.args.config_path:
            candidate = Path(self.args.config_path).expanduser()
            if candidate.exists():
                return candidate.resolve()
            raise FileNotFoundError(f"找不到設定檔：{candidate}")
        candidates = [
            self.script_dir / self.args.config_file_name,
            self.repo_root / self.args.config_file_name,
            self.script_dir / self.args.cli_config_file_name,
            self.repo_root / self.args.cli_config_file_name,
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate.resolve()
        raise FileNotFoundError(
            f"找不到設定檔，預期檔名為：{self.args.config_file_name} 或 {self.args.cli_config_file_name}"
        )

    @staticmethod
    def is_yaml_config(path: Path) -> bool:
        return path.suffix.lower() in {".yaml", ".yml"}

    @staticmethod
    def quote_yaml_string(value: object) -> str:
        return json.dumps("" if value is None else str(value), ensure_ascii=False)

    @staticmethod
    def dump_yaml_scalar(value: object) -> str:
        if value is None:
            return "null"
        if isinstance(value, bool):
            return "true" if value else "false"
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            return str(value)
        return MeshtasticAutoFlash.quote_yaml_string(value)

    @classmethod
    def dump_yaml_data(cls, value: object, indent: int = 0) -> list[str]:
        prefix = " " * indent
        if isinstance(value, dict):
            lines: list[str] = []
            for key, child in value.items():
                if isinstance(child, (dict, list)):
                    lines.append(f"{prefix}{key}:")
                    lines.extend(cls.dump_yaml_data(child, indent + 2))
                else:
                    lines.append(f"{prefix}{key}: {cls.dump_yaml_scalar(child)}")
            return lines or [f"{prefix}{{}}"]
        if isinstance(value, list):
            lines = []
            for child in value:
                if isinstance(child, dict):
                    lines.append(f"{prefix}-")
                    lines.extend(cls.dump_yaml_data(child, indent + 2))
                elif isinstance(child, list):
                    lines.append(f"{prefix}-")
                    lines.extend(cls.dump_yaml_data(child, indent + 2))
                else:
                    lines.append(f"{prefix}- {cls.dump_yaml_scalar(child)}")
            return lines or [f"{prefix}[]"]
        return [f"{prefix}{cls.dump_yaml_scalar(value)}"]

    @staticmethod
    def load_yaml_text(text: str) -> dict:
        import yaml

        data = yaml.safe_load(text) or {}
        if not isinstance(data, dict):
            raise RuntimeError("YAML 設定檔的根節點必須是 object/map。")
        return data

    def get_serial_ports(self) -> list:
        return list(serial.tools.list_ports.comports())

    def select_serial_port(self, preferred_port: Optional[str] = None) -> str:
        ports = self.get_serial_ports()
        if not ports:
            raise RuntimeError("未偵測到任何序列埠。")
        candidates = [
            port for port in ports
            if "VID:PID=303A" in (port.hwid or "").upper()
            or "USB" in (port.description or "").upper()
            or "USB" in port.device.upper()
        ]
        if preferred_port and any(port.device == preferred_port for port in candidates):
            return preferred_port
        if len(candidates) == 1:
            return candidates[0].device
        if len(candidates) > 1:
            self.log("偵測到多個 USB 序列埠：")
            for index, port in enumerate(candidates):
                self.log(f"[{index}] {port.device} ({port.description})")
            self.log("將自動使用清單中的第一個序列埠；如需指定，請先調整連接設備。")
            return candidates[0].device
        if len(ports) == 1:
            return ports[0].device
        raise RuntimeError("無法自動判斷要使用哪個序列埠。")

    def wait_for_serial_port(self, preferred_port: Optional[str]) -> str:
        timeout = self.args.port_detect_timeout_seconds if self.args.port_detect_timeout_seconds > 0 else 60
        poll = self.args.port_detect_interval_seconds if self.args.port_detect_interval_seconds > 0 else 2
        deadline = time.time() + timeout
        while True:
            try:
                return self.select_serial_port(preferred_port)
            except RuntimeError as exc:
                if "未偵測到任何序列埠" not in str(exc):
                    raise
                if time.time() >= deadline:
                    raise RuntimeError(f"等待 {timeout} 秒後仍未偵測到任何序列埠。") from exc
                self.log(f"尚未偵測到序列埠，{poll} 秒後重試...")
                time.sleep(poll)

    def wait_for_serial_port_with_user_reconnect(self, preferred_port: Optional[str], reason: str) -> str:
        while True:
            try:
                return self.wait_for_serial_port(preferred_port)
            except RuntimeError as exc:
                notify_user_attention(
                    "注意：裝置連線需要人工處理。\n"
                    f"{reason}\n"
                    f"{exc}\n"
                    "請重新插拔裝置後，按 Enter 繼續重試。"
                )
                try:
                    input()
                except EOFError:
                    time.sleep(2)

    def wait_for_operator_acknowledgement(self, reason: str) -> None:
        notify_user_attention(
            "注意：設定寫入已暫停。\n"
            f"{reason}\n"
            "請重新插拔裝置，確認系統已重新辨識後，按 Enter 繼續。"
        )
        try:
            input()
        except EOFError:
            time.sleep(2)

    def run_process(self, args: list[str], timeout: Optional[int] = None) -> tuple[int, str]:
        process = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        output_lines: list[str] = []
        start = time.time()
        while True:
            line = process.stdout.readline() if process.stdout else ""
            if line:
                text = line.rstrip("\n")
                output_lines.append(text)
                self.log(text, delay_after=False)
            if process.poll() is not None:
                break
            if timeout and (time.time() - start) > timeout:
                process.kill()
                raise subprocess.TimeoutExpired(args, timeout)
        return process.returncode, "\n".join(output_lines)

    @staticmethod
    def build_internal_tool_command(tool_name: str, tool_args: list[str]) -> list[str]:
        if getattr(sys, "frozen", False):
            return [sys.executable, INTERNAL_HELPER_FLAG, tool_name, *tool_args]
        return [sys.executable, str(Path(__file__).resolve()), INTERNAL_HELPER_FLAG, tool_name, *tool_args]

    def run_internal_tool(self, tool_name: str, tool_args: list[str], timeout: Optional[int] = None) -> tuple[int, str]:
        return self.run_process(self.build_internal_tool_command(tool_name, tool_args), timeout=timeout)

    def wait_for_meshtastic_ready(self, port: str) -> str:
        timeout = self.args.ready_timeout_seconds if self.args.ready_timeout_seconds > 0 else 30
        poll = self.args.ready_poll_seconds if self.args.ready_poll_seconds > 0 else 2
        cmd_timeout = self.args.ready_command_timeout_seconds if self.args.ready_command_timeout_seconds > 0 else 10
        tries = self.args.ready_retry_count if self.args.ready_retry_count > 0 else 1
        current_port = port
        for attempt in range(tries):
            deadline = time.time() + timeout
            while time.time() < deadline:
                try:
                    code, output = self.run_internal_tool(
                        "meshtastic",
                        ["--port", current_port, "--timeout", str(cmd_timeout), "--info"],
                        timeout=cmd_timeout,
                    )
                    if code == 0 and re.search(r"connected to radio", output, re.I):
                        self.log("裝置已就緒。")
                        return current_port
                except Exception:
                    pass
                if time.time() < deadline:
                    self.log(f"裝置尚未就緒，{poll} 秒後重試...")
                    time.sleep(poll)
            if attempt < tries - 1:
                self.log(f"等待 {timeout} 秒後裝置仍未就緒，重新偵測序列埠...")
                current_port = self.wait_for_serial_port(current_port)
                self.log(f"重新連線後使用序列埠：{current_port}")
        self.log(f"等待 {timeout} 秒後裝置仍未回應，將不再等待並繼續後續流程。")
        return current_port

    def invoke_esptool_flash(self, port: str, firmware: Path) -> None:
        self.log(f"開始刷寫韌體到 {port} ...")
        self.log("請確認裝置已進入可刷寫狀態，且 USB 連線穩定，不要在刷寫中拔除。")
        code, _ = self.run_internal_tool(
            "esptool",
            [
                "--chip",
                "esp32s3",
                "--port",
                port,
                "--baud",
                "115200",
                "--before",
                "default_reset",
                "--after",
                "hard_reset",
                "write_flash",
                "0x10000",
                str(firmware),
            ],
        )
        if code != 0:
            raise RuntimeError(f"esptool 執行失敗，結束代碼：{code}")

    @staticmethod
    def get_channels_block_from_text(text: str) -> list[str]:
        lines = text.splitlines()
        collect = False
        block: list[str] = []
        for line in lines:
            trimmed = line.strip()
            if not collect:
                if re.match(r"^Channels:", trimmed):
                    collect = True
                continue
            if not trimmed or re.match(r"^Primary channel URL:|^Complete URL", trimmed):
                break
            block.append(trimmed)
        return block

    @staticmethod
    def get_channel_urls_from_text(text: str) -> dict[str, Optional[str]]:
        primary = None
        complete = None
        for line in text.splitlines():
            trimmed = line.strip()
            primary_match = re.match(r"^Primary channel URL:\s*(\S+)$", trimmed)
            complete_match = re.match(r"^Complete URL[^:]*:\s*(\S+)$", trimmed)
            if primary_match:
                primary = primary_match.group(1).strip()
            if complete_match:
                complete = complete_match.group(1).strip()
        return {"Primary": primary, "Complete": complete}

    @staticmethod
    def normalize_channel_url(url: Optional[str]) -> Optional[str]:
        if not url:
            return None
        return url.strip().strip('"')

    def get_preferred_channel_url(self, urls: dict[str, Optional[str]]) -> Optional[str]:
        return self.normalize_channel_url(urls.get("Complete")) or self.normalize_channel_url(urls.get("Primary"))

    @staticmethod
    def parse_channel_line(line: str) -> Optional[dict]:
        match = re.match(r"^Index\s+(\d+):\s+(\w+)\s+psk=([^\s]+)\s+(\{.*\})$", line)
        if not match:
            return None
        import json

        settings = None
        try:
            settings = json.loads(match.group(4).strip())
        except Exception:
            settings = None
        return {
            "Index": int(match.group(1)),
            "Role": match.group(2),
            "PskTag": match.group(3),
            "Settings": settings,
            "Raw": line,
        }

    @staticmethod
    def _bool_to_str(value: object) -> Optional[str]:
        if value is None:
            return None
        if isinstance(value, bool):
            return str(value).lower()
        value_text = str(value)
        return value_text if value_text else None

    def new_channel_set_command(self, index: int, field: str, value: object) -> Optional[MeshtasticCommand]:
        value_text = self._bool_to_str(value)
        if value_text is None:
            return None
        escaped_value = value_text.replace('"', '\\"')
        raw_value = f'"{escaped_value}"' if re.search(r"\s", value_text) else value_text
        return MeshtasticCommand(
            type="SetChannelField",
            index=index,
            field=field,
            value=value_text,
            raw=f"meshtastic --ch-set {field} {raw_value} --ch-index {index}",
        )

    def get_channel_default_commands_from_text(self, text: str, index: int) -> list[MeshtasticCommand]:
        entry = None
        for line in self.get_channels_block_from_text(text):
            parsed = self.parse_channel_line(line)
            if parsed and parsed["Index"] == index:
                entry = parsed
                break
        if not entry:
            return []
        commands: list[MeshtasticCommand] = []
        psk_tag = (entry.get("PskTag") or "").lower()
        settings = entry.get("Settings") or {}
        if psk_tag in {"default", "none", "random"}:
            cmd = self.new_channel_set_command(index, "psk", psk_tag)
            if cmd:
                commands.append(cmd)
        elif settings.get("psk"):
            cmd = self.new_channel_set_command(index, "psk", settings.get("psk"))
            if cmd:
                commands.append(cmd)
        fields = [
            ("name", settings.get("name")),
            ("uplink_enabled", settings.get("uplinkEnabled")),
            ("downlink_enabled", settings.get("downlinkEnabled")),
            ("channel_num", settings.get("channelNum")),
            ("id", settings.get("id")),
        ]
        module_settings = settings.get("moduleSettings") or {}
        fields.extend(
            [
                ("module_settings.position_precision", module_settings.get("positionPrecision")),
                ("module_settings.is_client_muted", module_settings.get("isClientMuted")),
            ]
        )
        for field, value in fields:
            cmd = self.new_channel_set_command(index, field, value)
            if cmd:
                commands.append(cmd)
        return commands

    @staticmethod
    def insert_channel_commands_after_url(commands: list[MeshtasticCommand], channel_commands: list[MeshtasticCommand]) -> list[MeshtasticCommand]:
        if not channel_commands:
            return list(commands)
        last_channel_index = -1
        for index, command in enumerate(commands):
            if command.type in {"SetChannelUrl", "AddChannelUrl"}:
                last_channel_index = index
        if last_channel_index < 0:
            return list(commands) + list(channel_commands)
            return list(commands[: last_channel_index + 1]) + list(channel_commands) + list(commands[last_channel_index + 1 :])

    def build_config_from_cli_text(self, text: str, source_name: str = "CLI.md") -> dict:
        commands = self.get_meshtastic_commands_from_text(text)
        channels: list[dict] = []
        for line in self.get_channels_block_from_text(text):
            parsed = self.parse_channel_line(line)
            if not parsed:
                continue
            channels.append(
                {
                    "index": parsed["Index"],
                    "role": parsed["Role"],
                    "psk_tag": parsed["PskTag"],
                    "settings": parsed.get("Settings") or {},
                }
            )

        config = {
            "config_version": 1,
            "source": {
                "format": "cli-md",
                "path": source_name,
                "generated_at_runtime": True,
            },
            "firmware": {
                "preferred_file": self.args.firmware_file_name,
            },
            "flash": {
                "chip": "esp32s3",
                "baud": 115200,
                "before": "default_reset",
                "after": "hard_reset",
                "address": "0x10000",
            },
            "channel_urls": self.get_channel_urls_from_text(text),
            "channel_defaults": {
                "reapply_index": 3,
                "channels": channels,
            },
            "commands": [self.command_to_config_entry(command) for command in commands],
        }
        return config

    def export_cli_to_yaml(self, cli_path: Path, output_path: Path) -> Path:
        config = self.build_config_from_cli_text(self.read_text_file_best_encoding(cli_path), cli_path.name)
        yaml_text = "\n".join(self.dump_yaml_data(config)) + "\n"
        output_path.write_text(yaml_text, encoding="utf-8")
        return output_path

    @staticmethod
    def command_to_config_entry(command: MeshtasticCommand) -> dict:
        entry = {"type": command.type}
        if command.field is not None:
            entry["field"] = command.field
        if command.value is not None:
            entry["value"] = command.value
        if command.message is not None:
            entry["message"] = command.message
        if command.url is not None:
            entry["url"] = command.url
        if command.index is not None:
            entry["index"] = command.index
        if command.raw:
            entry["raw"] = command.raw
        return entry

    @staticmethod
    def command_from_config_entry(entry: dict) -> MeshtasticCommand:
        command_type = str(entry.get("type") or "").strip()
        if not command_type:
            raise RuntimeError("YAML commands 項目缺少 type。")
        command = MeshtasticCommand(
            type=command_type,
            raw=str(entry.get("raw") or "").strip(),
            field=entry.get("field"),
            value=None if entry.get("value") is None else str(entry.get("value")),
            message=None if entry.get("message") is None else str(entry.get("message")),
            url=None if entry.get("url") is None else str(entry.get("url")),
            index=None if entry.get("index") is None else int(entry.get("index")),
        )
        if command.raw:
            return command
        if command.type == "SetField":
            command.raw = f"meshtastic --set {command.field} {command.value}"
        elif command.type == "SetCannedMessage":
            command.raw = f"meshtastic --set-canned-message {command.message}"
        elif command.type == "SetChannelUrl":
            command.raw = f"meshtastic --ch-set-url {command.url}"
        elif command.type == "AddChannelUrl":
            command.raw = f"meshtastic --ch-add-url {command.url}"
        elif command.type == "SetChannelField":
            command.raw = f"meshtastic --ch-set {command.field} {command.value} --ch-index {command.index}"
        else:
            command.raw = command.type
        return command

    def build_channel_default_commands_from_config(self, config: dict) -> list[MeshtasticCommand]:
        channel_defaults = config.get("channel_defaults") or {}
        reapply_index = int(channel_defaults.get("reapply_index", 3))
        for entry in channel_defaults.get("channels") or []:
            if int(entry.get("index", -1)) != reapply_index:
                continue
            commands: list[MeshtasticCommand] = []
            psk_tag = str(entry.get("psk_tag") or "").lower()
            settings = entry.get("settings") or {}
            if psk_tag in {"default", "none", "random"}:
                cmd = self.new_channel_set_command(reapply_index, "psk", psk_tag)
                if cmd:
                    commands.append(cmd)
            elif settings.get("psk"):
                cmd = self.new_channel_set_command(reapply_index, "psk", settings.get("psk"))
                if cmd:
                    commands.append(cmd)
            fields = [
                ("name", settings.get("name")),
                ("uplink_enabled", settings.get("uplinkEnabled")),
                ("downlink_enabled", settings.get("downlinkEnabled")),
                ("channel_num", settings.get("channelNum")),
                ("id", settings.get("id")),
            ]
            module_settings = settings.get("moduleSettings") or {}
            fields.extend(
                [
                    ("module_settings.position_precision", module_settings.get("positionPrecision")),
                    ("module_settings.is_client_muted", module_settings.get("isClientMuted")),
                ]
            )
            for field, value in fields:
                cmd = self.new_channel_set_command(reapply_index, field, value)
                if cmd:
                    commands.append(cmd)
            return commands
        return []

    def get_meshtastic_commands(self, path: Path) -> list[MeshtasticCommand]:
        text = self.read_text_file_best_encoding(path)
        return self.get_meshtastic_commands_from_text(text)

    def get_meshtastic_commands_from_text(self, text: str) -> list[MeshtasticCommand]:
        commands: list[MeshtasticCommand] = []
        explicit_channel = False
        primary_url = None
        complete_url = None
        channel_insert_index: Optional[int] = None
        skip_channels_block = False
        section = None
        section_indent = 0
        top_level_keys = {"mqtt", "lora", "position", "device", "network", "display", "power", "bluetooth", "security", "canned_message", "cannedMessage"}

        for line in text.splitlines():
            expanded = line.replace("\t", "  ")
            trimmed = expanded.strip()
            if not trimmed:
                if skip_channels_block:
                    skip_channels_block = False
                if section:
                    section = None
                continue
            if re.match(r"^Channels:", trimmed):
                skip_channels_block = True
                section = None
                continue
            if skip_channels_block:
                continue
            match = re.match(r"^Primary channel URL:\s*(\S+)$", trimmed)
            if match:
                if not primary_url:
                    primary_url = match.group(1).strip()
                if channel_insert_index is None:
                    channel_insert_index = len(commands)
                continue
            match = re.match(r"^Complete URL[^:]*:\s*(\S+)$", trimmed)
            if match:
                complete_url = match.group(1).strip()
                if channel_insert_index is None:
                    channel_insert_index = len(commands)
                continue
            match = re.match(r"^meshtastic\s+--ch-set-url\s+(.+)$", trimmed)
            if match:
                explicit_channel = True
                url = match.group(1).strip()
                commands.append(MeshtasticCommand(type="SetChannelUrl", url=url, raw=trimmed))
                continue
            match = re.match(r"^meshtastic\s+--ch-add-url\s+(.+)$", trimmed)
            if match:
                explicit_channel = True
                url = match.group(1).strip()
                commands.append(MeshtasticCommand(type="AddChannelUrl", url=url, raw=trimmed))
                continue
            match = re.match(r"^meshtastic\s+--set-canned-message\s+(.+)$", trimmed)
            if match:
                message = match.group(1).strip().strip('"')
                commands.append(MeshtasticCommand(type="SetCannedMessage", message=message, raw=trimmed))
                continue
            match = re.match(r"^meshtastic\s+--set\s+(\S+)\s+(.+)$", trimmed)
            if match:
                field = match.group(1).strip()
                value = match.group(2).strip().strip('"')
                commands.append(MeshtasticCommand(type="SetField", field=field, value=value, raw=trimmed))
                continue

            indent = len(expanded) - len(expanded.lstrip())
            match = re.match(r'^\s*"?([A-Za-z0-9_]+)"?\s*:\s*\{\s*$', expanded)
            if match and match.group(1) in top_level_keys:
                section = match.group(1)
                section_indent = indent
                continue
            if section:
                if indent <= section_indent:
                    section = None
                    continue
                match = re.match(r'^\s*"?([A-Za-z0-9_]+)"?\s*:\s*(.+)$', expanded)
                if not match:
                    continue
                key = match.group(1)
                value = match.group(2).strip()
                if value in {"", "{", "["} or value.startswith(("{", "[")):
                    continue
                normalized_value = value.rstrip(",").strip().strip('"')
                if normalized_value in {"[]", "{}"}:
                    continue
                field = f"{section}.{key}"
                commands.append(MeshtasticCommand(type="SetField", field=field, value=normalized_value, raw=f"meshtastic --set {field} {normalized_value}"))

        if not explicit_channel and (complete_url or primary_url):
            url = complete_url or primary_url
            channel_command = MeshtasticCommand(type="SetChannelUrl", url=url, raw=f"meshtastic --ch-set-url {url}")
            if channel_insert_index is None or channel_insert_index <= 0:
                commands.insert(0, channel_command)
            elif channel_insert_index >= len(commands):
                commands.append(channel_command)
            else:
                commands = commands[:channel_insert_index] + [channel_command] + commands[channel_insert_index:]
        return commands

    def load_runtime_config(self, path: Path) -> tuple[list[MeshtasticCommand], Optional[str], list[MeshtasticCommand], dict]:
        text = self.read_text_file_best_encoding(path)
        if self.is_yaml_config(path):
            config = self.load_yaml_text(text)
            commands = [self.command_from_config_entry(entry) for entry in (config.get("commands") or [])]
            channel_urls = config.get("channel_urls") or {}
            expected_channel_url = self.get_preferred_channel_url(channel_urls)
            channel_default_commands = self.build_channel_default_commands_from_config(config)
            return commands, expected_channel_url, channel_default_commands, config

        config = self.build_config_from_cli_text(text, path.name)
        commands = [self.command_from_config_entry(entry) for entry in config.get("commands") or []]
        expected_channel_url = self.get_preferred_channel_url(config.get("channel_urls") or {})
        channel_default_commands = self.build_channel_default_commands_from_config(config)
        return commands, expected_channel_url, channel_default_commands, config

    @staticmethod
    def test_reboot_command(command: MeshtasticCommand) -> bool:
        if command.type in {"SetChannelUrl", "AddChannelUrl"}:
            return True
        return command.type == "SetField" and bool(command.field) and command.field.lower().startswith("lora.")

    @staticmethod
    def build_meshtastic_args(commands: list[MeshtasticCommand]) -> list[str]:
        args: list[str] = []
        channel_index = None
        for command in commands:
            if command.type == "SetField":
                args.extend(["--set", command.field or "", command.value or ""])
            elif command.type == "SetCannedMessage":
                args.extend(["--set-canned-message", command.message or ""])
            elif command.type == "SetChannelUrl":
                args.extend(["--ch-set-url", command.url or ""])
            elif command.type == "AddChannelUrl":
                args.extend(["--ch-add-url", command.url or ""])
            elif command.type == "SetChannelField":
                args.extend(["--ch-set", command.field or "", command.value or ""])
                if channel_index is None:
                    channel_index = command.index
                elif channel_index != command.index:
                    raise RuntimeError("Multiple channel indices in one meshtastic batch are not supported.")
        if channel_index is not None:
            args.extend(["--ch-index", str(channel_index)])
        return args

    @staticmethod
    def convert_camel_to_snake(value: str) -> str:
        return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value).lower() if value else value

    def get_field_ack_variants(self, field: str) -> list[str]:
        parts = field.split(".")
        if len(parts) <= 1:
            return [field]
        prefix = ".".join(parts[:-1]) + "."
        last = parts[-1]
        snake = self.convert_camel_to_snake(last)
        variants = [field]
        snake_field = prefix + snake
        if snake_field != field:
            variants.append(snake_field)
        return sorted(set(variants))

    def get_command_ack_patterns(self, command: MeshtasticCommand) -> list[str]:
        if command.type == "SetField" and command.field:
            return [rf"(?i)\bset\s+{re.escape(field)}\s+to\b" for field in self.get_field_ack_variants(command.field)]
        if command.type == "SetCannedMessage":
            return [r"(?i)\bsetting\s+canned\s+plugin\s+message\b", r"(?i)\bset\s+canned\s+message\b", r"(?i)\bsetting\s+canned\s+message\b"]
        return []

    def test_output_for_commands(self, output: str, commands: list[MeshtasticCommand]) -> tuple[bool, list[str]]:
        missing: list[str] = []
        if not output:
            return False, ["meshtastic CLI 沒有輸出任何內容"]
        if not re.search(r"connected to radio", output, re.I):
            missing.append("缺少：Connected to radio")
        for command in commands:
            patterns = self.get_command_ack_patterns(command)
            if patterns and not any(re.search(pattern, output) for pattern in patterns):
                missing.append(f"缺少命令確認訊息：{command.raw}")
        return len(missing) == 0, missing

    @staticmethod
    def test_connection_issue(output: str) -> bool:
        return bool(re.search(r"serial port disconnected|timed out waiting for connection completion|ClearCommError failed|PermissionError\(13\)", output or "", re.I))

    @staticmethod
    def test_manual_reconnect_required(output: str) -> bool:
        return bool(re.search(r"serial device couldn't be opened|could not open port|FileNotFoundError|in use by another process", output or "", re.I))

    def invoke_meshtastic_capture(self, port: str, meshtastic_args: list[str], raw: str) -> str:
        current_port = port
        for attempt in range(1, max(1, self.args.meshtastic_retry_count) + 1):
            try:
                code, output = self.run_internal_tool(
                    "meshtastic",
                    ["--port", current_port, "--timeout", str(self.args.meshtastic_timeout_seconds), *meshtastic_args],
                    timeout=self.args.meshtastic_timeout_seconds,
                )
            except Exception as exc:
                code, output = 1, str(exc)
            if code == 0:
                return output
            if attempt < self.args.meshtastic_retry_count:
                self.log(f"meshtastic 指令失敗（第 {attempt}/{self.args.meshtastic_retry_count} 次），{self.args.meshtastic_retry_delay_seconds} 秒後重試...")
                time.sleep(max(0, self.args.meshtastic_retry_delay_seconds))
                continue
            raise RuntimeError(f"meshtastic 指令失敗：{raw}\n{output}")
        raise RuntimeError(f"meshtastic 指令失敗：{raw}")

    def invoke_meshtastic_with_retry(
        self,
        port: str,
        meshtastic_args: list[str],
        raw: str,
        expected_commands: Optional[list[MeshtasticCommand]] = None,
        require_ack: bool = False,
    ) -> str:
        current_port = port
        retry_count = max(1, self.args.meshtastic_retry_count)
        while True:
            last_output = ""
            for attempt in range(1, retry_count + 1):
                try:
                    code, output = self.run_internal_tool(
                        "meshtastic",
                        ["--port", current_port, "--timeout", str(self.args.meshtastic_timeout_seconds), *meshtastic_args],
                        timeout=self.args.meshtastic_timeout_seconds,
                    )
                except Exception as exc:
                    code, output = 1, str(exc)

                last_output = output
                success = code == 0
                if success and require_ack:
                    success, missing = self.test_output_for_commands(output, expected_commands or [])
                    if not success and missing:
                        self.log("缺少預期的 CLI 回應：\n" + "\n".join(missing))
                if success:
                    return current_port
                manual_reconnect = self.test_manual_reconnect_required(output)
                connection_issue = self.test_connection_issue(output)
                if attempt < retry_count:
                    if manual_reconnect:
                        self.log("序列埠開啟失敗，請檢查是否被其他程式占用，或重新插拔裝置。")
                        current_port = self.wait_for_serial_port_with_user_reconnect(
                            current_port,
                            "系統目前無法重新開啟裝置序列埠。",
                        )
                        self.log(f"重新連線後使用序列埠：{current_port}")
                        current_port = self.wait_for_meshtastic_ready(current_port)
                        continue
                    if connection_issue:
                        self.log("偵測到序列連線異常，請等待裝置重新枚舉或手動重新插拔。")
                        current_port = self.wait_for_serial_port_with_user_reconnect(
                            current_port,
                            "設定寫入期間發生序列連線異常。",
                        )
                        self.log(f"重新連線後使用序列埠：{current_port}")
                        current_port = self.wait_for_meshtastic_ready(current_port)
                        continue
                    if self.args.meshtastic_retry_delay_seconds > 0:
                        self.log(f"meshtastic 指令失敗（第 {attempt}/{retry_count} 次），{self.args.meshtastic_retry_delay_seconds} 秒後重試...")
                        time.sleep(self.args.meshtastic_retry_delay_seconds)
                    else:
                        self.log(f"meshtastic 指令失敗（第 {attempt}/{retry_count} 次），立即重試...")
                    continue

            self.wait_for_operator_acknowledgement(
                "裝置多次無法寫入設定，流程不會結束。\n"
                "我會在你重新插拔裝置後，繼續從目前步驟重試。"
            )
            current_port = self.wait_for_serial_port_with_user_reconnect(
                current_port,
                "請確認裝置已重新插拔並重新出現在系統中。",
            )
            self.log(f"使用者介入後重新連線到序列埠：{current_port}")
            current_port = self.wait_for_meshtastic_ready(current_port)
            if last_output:
                self.log("前一次失敗輸出摘要如下：")
                self.log(last_output, delay_after=False)

    def invoke_meshtastic_commands(self, port: str, commands: list[MeshtasticCommand]) -> str:
        reboot_commands = [command for command in commands if self.test_reboot_command(command)]
        normal_commands = [command for command in commands if not self.test_reboot_command(command)]
        current_port = port

        if reboot_commands:
            batch_size = self.args.reboot_batch_size if self.args.reboot_batch_size > 0 else 1
            batch_index = 1
            for start in range(0, len(reboot_commands), batch_size):
                batch = reboot_commands[start : start + batch_size]
                self.log(f"執行會觸發重開機的第 {batch_index} 批設定（共 {len(batch)} 筆）...")
                for command in batch:
                    self.log(f"排入命令：{command.raw}")
                meshtastic_args = self.build_meshtastic_args(batch)
                self.log(f"執行命令：meshtastic --port {current_port} {' '.join(shlex.quote(arg) for arg in meshtastic_args)}")
                current_port = self.invoke_meshtastic_with_retry(current_port, meshtastic_args, "\n".join(command.raw for command in batch), batch, True)
                batch_index += 1
                if self.args.reboot_wait_seconds > 0:
                    self.log(f"裝置可能正在重開機，等待 {self.args.reboot_wait_seconds} 秒後再檢查...")
                    time.sleep(self.args.reboot_wait_seconds)
                    current_port = self.wait_for_serial_port(current_port)
                    self.log(f"重開機後使用序列埠：{current_port}")
                    current_port = self.wait_for_meshtastic_ready(current_port)

        if normal_commands:
            needs_transaction = self.args.use_transaction and any(command.type in {"SetField", "SetCannedMessage"} for command in normal_commands)
            if needs_transaction:
                self.log("開啟設定交易模式...")
                current_port = self.invoke_meshtastic_with_retry(current_port, ["--begin-edit"], "meshtastic --begin-edit")
            self.log(f"執行不會觸發重開機的設定（共 {len(normal_commands)} 筆）...")
            for command in normal_commands:
                self.log(f"排入命令：{command.raw}")
            meshtastic_args = self.build_meshtastic_args(normal_commands)
            self.log(f"執行命令：meshtastic --port {current_port} {' '.join(shlex.quote(arg) for arg in meshtastic_args)}")
            current_port = self.invoke_meshtastic_with_retry(current_port, meshtastic_args, "\n".join(command.raw for command in normal_commands), normal_commands, True)
            if needs_transaction:
                self.log("送出設定交易...")
                current_port = self.invoke_meshtastic_with_retry(current_port, ["--commit-edit"], "meshtastic --commit-edit")

        return current_port

    def normalize_info_path(self, path: str) -> Optional[str]:
        if not path:
            return path
        if path.startswith("Preferences."):
            path = path[len("Preferences.") :]
        if path.startswith("Module preferences."):
            path = path[len("Module preferences.") :]
        if path.startswith("cannedMessage."):
            suffix = path[len("cannedMessage.") :]
            parts = [self.convert_camel_to_snake(part) for part in suffix.split(".")]
            path = "canned_message." + ".".join(parts)
        return path

    def convert_info_to_map(self, info_text: str) -> dict[str, str]:
        info_map: dict[str, str] = {}
        stack: list[tuple[int, str]] = []
        for line in info_text.splitlines():
            expanded = line.replace("\t", "  ")
            match = re.match(r'^\s*"?([A-Za-z0-9_ ]+)"?\s*:\s*(.*)$', expanded)
            if not match:
                continue
            indent = len(expanded) - len(expanded.lstrip())
            if indent <= 1:
                stack = []
            key = match.group(1)
            value = match.group(2).strip()
            stack = [entry for entry in stack if entry[0] < indent]
            normalized_value = value.strip().rstrip(",")
            if normalized_value in {"", "{", "["}:
                stack.append((indent, key))
                continue
            path_parts = [entry[1] for entry in stack if entry[1]]
            path_parts.append(key)
            full_path = self.normalize_info_path(".".join(path_parts))
            normalized_value = normalized_value.strip('"')
            if full_path:
                info_map[full_path] = normalized_value
        return info_map

    @staticmethod
    def normalize_value(value: str) -> str:
        normalized = value.strip().strip('"').rstrip(",").lower()
        try:
            return f"num:{float(normalized):.15g}"
        except ValueError:
            return normalized

    def get_missing_commands(self, commands: list[MeshtasticCommand], info_map: dict[str, str]) -> tuple[list[MeshtasticCommand], list[MeshtasticCommand]]:
        missing: list[MeshtasticCommand] = []
        unverified: list[MeshtasticCommand] = []
        for command in commands:
            if command.type != "SetField" or not command.field or command.value is None:
                continue
            if command.field not in info_map:
                unverified.append(command)
                continue
            expected = self.normalize_value(command.value)
            actual = self.normalize_value(info_map[command.field])
            if expected != actual:
                missing.append(command)
        return missing, unverified

    def get_info_channel_url(self, info_text: str) -> Optional[str]:
        return self.get_preferred_channel_url(self.get_channel_urls_from_text(info_text))

    def get_channel_url_command_if_mismatch(self, expected_url: Optional[str], actual_url: Optional[str]) -> Optional[MeshtasticCommand]:
        expected = self.normalize_channel_url(expected_url)
        actual = self.normalize_channel_url(actual_url)
        if expected and actual and expected != actual:
            return MeshtasticCommand(type="SetChannelUrl", url=expected, raw=f"meshtastic --ch-set-url {expected}")
        return None

    @staticmethod
    def get_expected_canned_message(commands: list[MeshtasticCommand]) -> Optional[str]:
        messages = [command.message for command in commands if command.type == "SetCannedMessage" and command.message]
        return messages[-1] if messages else None

    @staticmethod
    def get_canned_message_from_output(output: str) -> Optional[str]:
        match = re.search(r"(?im)^canned_plugin_message\s*:\s*(.+)$", output or "")
        if match:
            return match.group(1).strip()
        lines = [line.strip() for line in (output or "").splitlines() if line.strip() and not re.match(r"(?i)^connected to radio$", line.strip())]
        return lines[-1] if lines else None

    def get_canned_message_from_device(self, port: str) -> Optional[str]:
        output = self.invoke_meshtastic_capture(port, ["--get-canned-message"], "meshtastic --get-canned-message")
        return self.get_canned_message_from_output(output)

    def get_meshtastic_info(self, port: str) -> str:
        return self.invoke_meshtastic_capture(port, ["--info"], "meshtastic --info")

    def verify_device_settings(
        self,
        port: str,
        commands: list[MeshtasticCommand],
        expected_channel_url: Optional[str],
        channel_default_commands: list[MeshtasticCommand],
    ) -> str:
        max_passes = self.args.reapply_max_passes if self.args.reapply_max_passes > 0 else 1
        current_port = port
        verified = False
        for current_pass in range(1, max_passes + 1):
            self.log(f"開始驗證裝置設定（第 {current_pass}/{max_passes} 輪）...")
            info_text = self.get_meshtastic_info(current_port)
            info_map = self.convert_info_to_map(info_text)
            missing, unverified = self.get_missing_commands(commands, info_map)
            actual_channel_url = self.get_info_channel_url(info_text)
            channel_command = self.get_channel_url_command_if_mismatch(expected_channel_url, actual_channel_url)
            if not missing and not channel_command:
                self.log("裝置設定驗證完成，內容一致。")
                if unverified:
                    self.log(f"有 {len(unverified)} 筆設定不在 --info 中，已略過驗證。")
                verified = True
                break
            for command in missing:
                actual = info_map.get(command.field or "", "<info 中沒有此欄位>")
                self.log(f"設定不一致：{command.raw}（預期={command.value}，實際={actual}）")
            if channel_command:
                self.log(f"偵測到 Channel URL 不一致，準備重套：{channel_command.raw}")
            if unverified:
                self.log(f"有 {len(unverified)} 筆設定不在 --info 中，已略過驗證。")
            reapply = list(missing)
            if channel_command:
                reapply.append(channel_command)
                reapply.extend(channel_default_commands)
            if reapply and current_pass < max_passes:
                self.log(f"重新套用 {len(reapply)} 筆不一致設定...")
                current_port = self.invoke_meshtastic_commands(current_port, reapply)
                time.sleep(2)
        if not verified:
            raise RuntimeError(f"設定驗證失敗，已重試 {max_passes} 輪。")
        return current_port

    def verify_canned_message(self, port: str, commands: list[MeshtasticCommand]) -> str:
        expected = self.get_expected_canned_message(commands)
        if not expected:
            return port
        current_port = port
        max_passes = self.args.reapply_max_passes if self.args.reapply_max_passes > 0 else 1
        for current_pass in range(1, max_passes + 1):
            self.log(f"開始驗證罐頭訊息（第 {current_pass}/{max_passes} 輪）...")
            actual = self.get_canned_message_from_device(current_port)
            if actual == expected:
                self.log("罐頭訊息驗證完成，內容一致。")
                return current_port
            self.log(f'罐頭訊息不一致：預期="{expected}"，實際="{actual or "<無內容>"}"')
            if current_pass < max_passes:
                command = MeshtasticCommand(type="SetCannedMessage", message=expected, raw=f"meshtastic --set-canned-message {expected}")
                self.log("重新套用罐頭訊息...")
                current_port = self.invoke_meshtastic_with_retry(current_port, ["--set-canned-message", expected], command.raw, [command], True)
                time.sleep(2)
        raise RuntimeError(f"罐頭訊息驗證失敗，已重試 {max_passes} 輪。")

    def ensure_dependencies(self) -> None:
        for command in ("meshtastic",):
            if not shutil.which(command):
                raise RuntimeError(f"系統找不到 `{command}`，請先安裝並確認它在 PATH 中。")

    @staticmethod
    def build_pip_install_command(package: str) -> str:
        return subprocess.list2cmdline([sys.executable, "-m", "pip", "install", package])

    def ensure_dependencies(self) -> None:
        package_names = {
            "meshtastic": "meshtastic",
            "esptool": "esptool",
            "serial": "pyserial",
            "yaml": "PyYAML",
        }
        missing = [package for module_name, package in package_names.items() if importlib.util.find_spec(module_name) is None]
        if not missing:
            return
        if getattr(sys, "frozen", False):
            missing_text = ", ".join(missing)
            raise RuntimeError(f"Bundled runtime dependency is missing: {missing_text}")
        install_commands = [self.build_pip_install_command(package) for package in missing]
        raise RuntimeError("Missing Python packages:\n" + "\n".join(install_commands))

    def run(self) -> None:
        self.print_startup_banner()
        self.ensure_dependencies()

        if self.args.export_config_yaml:
            cli_path = self.resolve_cli_config_path()
            output_path = Path(self.args.export_config_yaml).expanduser()
            self.export_cli_to_yaml(cli_path, output_path)
            self.log(f"已輸出 YAML 設定檔：{output_path}")
            return

        config_path = self.resolve_config_path()
        commands, expected_channel_url, channel_default_commands, config = self.load_runtime_config(config_path)
        commands = self.insert_channel_commands_after_url(commands, channel_default_commands)
        if not commands:
            raise RuntimeError(f"在設定檔中找不到任何 meshtastic 設定命令：{config_path}")

        firmware_name = ((config.get("firmware") or {}).get("preferred_file") or "").strip()
        firmware_path = self.resolve_firmware_path(firmware_name)
        self.log("=== Meshtastic 自動刷寫流程開始 ===")
        self.log(f"本次使用韌體：{firmware_path}")
        self.log(f"本次使用設定檔：{config_path}")
        if firmware_name and firmware_name != firmware_path.name:
            self.log(f"設定檔偏好的韌體名稱為 {firmware_name}，目前實際使用 {firmware_path.name}。")

        initial_port = self.select_serial_port()
        self.log(f"目前使用序列埠：{initial_port}")
        self.log("下一步將開始刷寫韌體，請不要拔掉 USB，也不要關閉其他需要的驅動程式。")

        self.invoke_esptool_flash(initial_port, firmware_path)
        self.log(f"韌體刷寫完成，等待 {self.args.post_flash_wait_seconds} 秒讓裝置重新開機...")
        self.log("等待裝置重新掛載期間，若系統有重新抓驅動或序列埠變更，屬於正常現象。")
        time.sleep(max(0, self.args.post_flash_wait_seconds))

        port_after = self.wait_for_serial_port(initial_port)
        self.log(f"重開機後偵測到序列埠：{port_after}")
        port_after = self.wait_for_meshtastic_ready(port_after)
        self.log(f"共解析出 {len(commands)} 筆設定命令，開始寫入裝置。")

        port_after = self.invoke_meshtastic_commands(port_after, commands)

        if self.args.reboot_after_config:
            self.log("設定已寫入，準備重啟裝置並再次驗證。")
            port_after = self.invoke_meshtastic_with_retry(port_after, ["--reboot"], "meshtastic --reboot")
            self.log(f"等待 {self.args.post_config_reboot_wait_seconds} 秒讓裝置完成重啟...")
            time.sleep(max(0, self.args.post_config_reboot_wait_seconds))
            port_after = self.wait_for_serial_port(port_after)
            self.log(f"設定後重啟序列埠：{port_after}")
            port_after = self.wait_for_meshtastic_ready(port_after)

        port_after = self.verify_device_settings(port_after, commands, expected_channel_url, channel_default_commands)
        port_after = self.verify_canned_message(port_after, commands)

        force_fields = [
            ("canned_message.inputbroker_pin_a", "37"),
            ("canned_message.inputbroker_pin_b", "26"),
            ("canned_message.inputbroker_pin_press", "4"),
            ("canned_message.inputbroker_event_cw", "UP"),
            ("canned_message.inputbroker_event_ccw", "DOWN"),
            ("canned_message.inputbroker_event_press", "SELECT"),
            ("canned_message.enabled", "true"),
            ("canned_message.rotary1_enabled", "true"),
            ("canned_message.allow_input_source", "rotEnc1"),
        ]
        force_commands = [MeshtasticCommand(type="SetField", field=field, value=value, raw=f"meshtastic --set {field} {value}") for field, value in force_fields]
        self.log("最後補寫 canned_message input broker 相關欄位。")
        self.invoke_meshtastic_with_retry(
            port_after,
            self.build_meshtastic_args(force_commands),
            " ".join(command.raw for command in force_commands),
            force_commands,
            True,
        )
        self.log("=== 自動刷寫與設定完成 ===")
        self.log("若要分發給其他人使用，請搭配對應平台的打包輸出資料夾。")


def main() -> int:
    helper_exit_code = try_run_internal_helper(sys.argv[1:])
    if helper_exit_code is not None:
        return helper_exit_code
    try:
        MeshtasticAutoFlash().run()
        pause_before_exit("\u8a2d\u5b9a\u5b8c\u7562!\u73fe\u5728\u4f60\u53ef\u4ee5\u958b\u59cb\u4f60\u7684\u65c5\u7a0b\u4e86!\n\u6309\u4efb\u610f\u9375\u96e2\u958b")
        return 0
    except KeyboardInterrupt:
        LOGGER.info("使用者已中斷流程。")
        return 130
    except Exception as exc:
        LOGGER.error(str(exc))
        pause_before_exit("失敗，請查看log並提交給團隊做確認，按任意鍵退出")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
