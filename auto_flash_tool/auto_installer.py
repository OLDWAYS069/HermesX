#!/usr/bin/env python3
"""
Meshtastic 自動刷寫與設定工具
以 flash_and_config.ps1 的流程為基準實作 Python 版本。
"""

from __future__ import annotations

import argparse
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
        self.script_dir = Path(__file__).resolve().parent
        self.repo_root = self._find_repo_root(self.script_dir)
        self.args = self._parse_arguments()
        self.log_path = self._initialize_log()

    def _parse_arguments(self) -> argparse.Namespace:
        parser = argparse.ArgumentParser(description="Meshtastic 自動刷寫和設定工具")
        parser.add_argument("--firmware-path", default="", help="韌體檔案路徑")
        parser.add_argument("--firmware-file-name", default="HermesX_0.2.8-beta0002-update.bin", help="韌體檔案名稱")
        parser.add_argument("--cli-config-path", default="", help="CLI 設定檔案路徑")
        parser.add_argument("--cli-config-file-name", default="CLI.md", help="CLI 設定檔案名稱")
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
        log_path = Path(self.args.log_path).expanduser() if self.args.log_path else self.script_dir / "flash_and_config.log"
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

    def log(self, message: str) -> None:
        LOGGER.info(message)

    def print_startup_banner(self) -> None:
        banner_path = self.repo_root / "ascii-art-text-1773857730689.txt"
        if not banner_path.exists():
            return
        try:
            print(self.read_text_file_best_encoding(banner_path))
        except Exception as exc:
            self.log(f"讀取開場 ASCII 圖失敗：{exc}")

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

    def resolve_firmware_path(self) -> Path:
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
                self.log(text)
            if process.poll() is not None:
                break
            if timeout and (time.time() - start) > timeout:
                process.kill()
                raise subprocess.TimeoutExpired(args, timeout)
        return process.returncode, "\n".join(output_lines)

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
                    code, output = self.run_process(
                        ["meshtastic", "--port", current_port, "--timeout", str(cmd_timeout), "--info"],
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
        code, _ = self.run_process(
            [
                sys.executable,
                "-m",
                "esptool",
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
            ]
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
        raw_value = f"\"{value_text.replace('\"', '\\\"')}\"" if re.search(r"\s", value_text) else value_text
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

    def get_meshtastic_commands(self, path: Path) -> list[MeshtasticCommand]:
        text = self.read_text_file_best_encoding(path)
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
                code, output = self.run_process(
                    ["meshtastic", "--port", current_port, "--timeout", str(self.args.meshtastic_timeout_seconds), *meshtastic_args],
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
        for attempt in range(1, retry_count + 1):
            try:
                code, output = self.run_process(
                    ["meshtastic", "--port", current_port, "--timeout", str(self.args.meshtastic_timeout_seconds), *meshtastic_args],
                    timeout=self.args.meshtastic_timeout_seconds,
                )
            except Exception as exc:
                code, output = 1, str(exc)

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
                    current_port = self.wait_for_serial_port(current_port)
                    self.log(f"重新連線後使用序列埠：{current_port}")
                    current_port = self.wait_for_meshtastic_ready(current_port)
                    continue
                if connection_issue:
                    self.log("偵測到序列連線異常，請等待裝置重新枚舉或手動重新插拔。")
                    current_port = self.wait_for_serial_port(current_port)
                    self.log(f"重新連線後使用序列埠：{current_port}")
                    current_port = self.wait_for_meshtastic_ready(current_port)
                    continue
                if self.args.meshtastic_retry_delay_seconds > 0:
                    self.log(f"meshtastic 指令失敗（第 {attempt}/{retry_count} 次），{self.args.meshtastic_retry_delay_seconds} 秒後重試...")
                    time.sleep(self.args.meshtastic_retry_delay_seconds)
                else:
                    self.log(f"meshtastic 指令失敗（第 {attempt}/{retry_count} 次），立即重試...")
                continue
            raise RuntimeError(f"meshtastic 指令失敗：{raw}\n{output}")
        raise RuntimeError(f"meshtastic 指令失敗：{raw}")

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

    def run(self) -> None:
        self.print_startup_banner()
        self.ensure_dependencies()
        if not shutil.which(sys.executable):
            raise RuntimeError("系統找不到 Python，請先確認 Python 已安裝且在 PATH 中。")

        firmware_path = self.resolve_firmware_path()
        cli_config_path = self.resolve_cli_config_path()
        self.log("=== Meshtastic 自動刷寫流程開始 ===")
        self.log(f"本次使用韌體：{firmware_path}")
        self.log(f"本次使用 CLI 設定：{cli_config_path}")

        cli_text = self.read_text_file_best_encoding(cli_config_path)
        expected_channel_url = self.get_preferred_channel_url(self.get_channel_urls_from_text(cli_text))
        channel_default_commands = self.get_channel_default_commands_from_text(cli_text, 3)

        initial_port = self.select_serial_port()
        self.log(f"目前使用序列埠：{initial_port}")
        self.log("下一步將開始刷寫韌體，請不要拔掉 USB，也不要關閉其他需要的驅動程式。")

        self.invoke_esptool_flash(initial_port, firmware_path)
        self.log(f"韌體刷寫完成，等待 {self.args.post_flash_wait_seconds} 秒讓裝置重新開機...")
        self.log("如果裝置重啟較慢，請保持連線並等待，不要立刻重新插拔。")
        time.sleep(max(0, self.args.post_flash_wait_seconds))

        port_after = self.wait_for_serial_port(initial_port)
        self.log(f"重開機後偵測到的序列埠：{port_after}")
        port_after = self.wait_for_meshtastic_ready(port_after)

        commands = self.get_meshtastic_commands(cli_config_path)
        if not commands:
            raise RuntimeError(f"在 CLI 設定檔中找不到任何 meshtastic 設定命令：{cli_config_path}")
        commands = self.insert_channel_commands_after_url(commands, channel_default_commands)
        self.log(f"總共排入 {len(commands)} 筆設定命令，下一步開始下發設定。")

        port_after = self.invoke_meshtastic_commands(port_after, commands)

        if self.args.reboot_after_config:
            self.log("設定已下發完成，下一步主動重開機讓設定完整生效...")
            port_after = self.invoke_meshtastic_with_retry(port_after, ["--reboot"], "meshtastic --reboot")
            self.log(f"等待 {self.args.post_config_reboot_wait_seconds} 秒讓設定後重開機完成...")
            time.sleep(max(0, self.args.post_config_reboot_wait_seconds))
            port_after = self.wait_for_serial_port(port_after)
            self.log(f"設定重開機後使用序列埠：{port_after}")
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
        self.log("最後一步：強制寫入 canned_message input broker 相關設定...")
        self.invoke_meshtastic_with_retry(
            port_after,
            self.build_meshtastic_args(force_commands),
            " ".join(command.raw for command in force_commands),
            force_commands,
            True,
        )
        self.log("=== 所有命令執行完成 ===")
        self.log("接下來請實際檢查裝置是否正常開機、可連線、且設定值已生效。")


def main() -> int:
    try:
        MeshtasticAutoFlash().run()
        return 0
    except KeyboardInterrupt:
        LOGGER.info("使用者已中斷流程。")
        return 130
    except Exception as exc:
        LOGGER.error(str(exc))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
