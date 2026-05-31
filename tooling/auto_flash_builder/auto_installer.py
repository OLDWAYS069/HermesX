#!/usr/bin/env python3
"""
Meshtastic ?芸??瑕神?身摰極??
隞?flash_and_config.ps1 ??蝔?箸?撖虫? Python ???
"""

from __future__ import annotations

import argparse
import ctypes
import importlib.util
import json
import logging
import io
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


class SafeConsoleHandler(logging.StreamHandler):
    def emit(self, record: logging.LogRecord) -> None:
        try:
            super().emit(record)
        except UnicodeEncodeError:
            try:
                msg = self.format(record)
                stream = self.stream
                encoding = getattr(stream, "encoding", None) or "utf-8"
                safe = msg.encode(encoding, errors="replace").decode(encoding, errors="replace")
                stream.write(safe + self.terminator)
                self.flush()
            except Exception:
                pass


INTERNAL_HELPER_FLAG = "--internal-cli"
LOG_LINE_DELAY_SECONDS = 0.5
STARTUP_MUSIC_VOLUME = 0.2
STARTUP_MUSIC_DUCK_FACTOR = 0.5
PROMPT_SOUND_VOLUME = 0.8
SUCCESS_SOUND_FILENAMES = ("success.mp3", "freesound_community-success-fanfare-trumpets-6185.mp3")
ERROR_SOUND_FILENAMES = ("error.mp3", "universfield-error-010-206498.mp3")
WARNING_SOUND_FILENAMES = ("warning.mp3", "freesound_community-beep-warning-6387.mp3")
HERMESX_SUCCESS_TONES = ((587, 100), (784, 120))
HERMESX_FAILED_TONES = ((659, 80), (554, 100))
WARNING_TONES = ((988, 120), (0, 80), (988, 120))
STARTUP_MUSIC_PROCESS = None
STARTUP_MUSIC_CONTROL_PATH = None


def configure_stdio_for_unicode() -> None:
    for stream_name in ("stdout", "stderr"):
        stream = getattr(sys, stream_name, None)
        if stream is None:
            continue
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass


def normalize_exit_code(code: object) -> int:
    if code is None:
        return 0
    if isinstance(code, bool):
        return int(code)
    if isinstance(code, int):
        return code
    return 1


def set_windows_console_green() -> None:
    if os.name != "nt":
        return
    try:
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)
        if handle in (0, -1):
            return
        kernel32.SetConsoleTextAttribute(handle, 0x0A)
    except Exception:
        pass


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


def is_windows_process_running(pid: int) -> bool:
    if os.name != "nt" or pid <= 0:
        return False
    try:
        kernel32 = ctypes.windll.kernel32
        process_handle = kernel32.OpenProcess(0x00100000, False, pid)
        if not process_handle:
            return False
        try:
            wait_result = kernel32.WaitForSingleObject(process_handle, 0)
            return wait_result == 0x00000102
        finally:
            kernel32.CloseHandle(process_handle)
    except Exception:
        return False


def load_startup_music_state(control_path: Optional[Path]) -> dict:
    if control_path is None or not control_path.exists():
        return {}
    try:
        data = json.loads(control_path.read_text(encoding="utf-8").lstrip("\ufeff"))
        return data if isinstance(data, dict) else {}
    except Exception:
        return {}


def write_startup_music_state(volume: Optional[float] = None, stop: Optional[bool] = None) -> None:
    global STARTUP_MUSIC_CONTROL_PATH
    if STARTUP_MUSIC_CONTROL_PATH is None:
        return
    state = load_startup_music_state(STARTUP_MUSIC_CONTROL_PATH)
    if volume is not None:
        state["volume"] = max(0.0, min(1.0, float(volume)))
    if stop is not None:
        state["stop"] = bool(stop)
    try:
        STARTUP_MUSIC_CONTROL_PATH.write_text(json.dumps(state), encoding="utf-8")
    except Exception:
        pass


def set_startup_music_volume(volume: float) -> None:
    write_startup_music_state(volume=volume, stop=False)


def duck_startup_music() -> None:
    set_startup_music_volume(STARTUP_MUSIC_VOLUME * STARTUP_MUSIC_DUCK_FACTOR)


def restore_startup_music_volume() -> None:
    set_startup_music_volume(STARTUP_MUSIC_VOLUME)


def play_audio_file(
    path_text: str,
    volume_text: str = "0.2",
    parent_pid_text: str = "",
    control_path_text: str = "",
    loop_text: str = "0",
) -> int:
    music_path = Path(path_text).expanduser()
    if not music_path.exists():
        return 2
    parent_pid = int(parent_pid_text) if str(parent_pid_text).strip().isdigit() else 0
    control_path = Path(control_path_text).expanduser() if str(control_path_text).strip() else None
    loop_playback = str(loop_text).strip() in {"1", "true", "True", "yes"}
    try:
        os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")
        import pygame

        base_volume = max(0.0, min(1.0, float(volume_text)))
        current_volume = base_volume
        pygame.mixer.init()
        pygame.mixer.music.load(str(music_path.resolve()))
        pygame.mixer.music.set_volume(current_volume)
        pygame.mixer.music.play(-1 if loop_playback else 0)
        while True:
            if parent_pid and not is_windows_process_running(parent_pid):
                pygame.mixer.music.stop()
                return 0
            state = load_startup_music_state(control_path)
            if state.get("stop"):
                pygame.mixer.music.stop()
                return 0
            desired_volume = max(0.0, min(1.0, float(state.get("volume", base_volume))))
            if abs(desired_volume - current_volume) > 0.001:
                pygame.mixer.music.set_volume(desired_volume)
                current_volume = desired_volume
            if not loop_playback and not pygame.mixer.music.get_busy():
                return 0
            if loop_playback and not pygame.mixer.music.get_busy():
                pygame.mixer.music.play(-1)
            time.sleep(0.2)
    except Exception:
        return play_audio_file_with_winmm(path_text, volume_text, parent_pid_text, control_path_text, loop_text)
    finally:
        try:
            import pygame

            pygame.mixer.music.stop()
            pygame.mixer.quit()
        except Exception:
            pass


def play_audio_file_with_winmm(
    path_text: str,
    volume_text: str = "0.2",
    parent_pid_text: str = "",
    control_path_text: str = "",
    loop_text: str = "0",
) -> int:
    if os.name != "nt":
        return 1
    music_path = Path(path_text).expanduser()
    if not music_path.exists():
        return 2
    parent_pid = int(parent_pid_text) if str(parent_pid_text).strip().isdigit() else 0
    control_path = Path(control_path_text).expanduser() if str(control_path_text).strip() else None
    loop_playback = str(loop_text).strip() in {"1", "true", "True", "yes"}
    alias = f"codex_startup_{os.getpid()}"
    winmm = ctypes.windll.winmm
    status_buffer = ctypes.create_unicode_buffer(256)
    base_volume = max(0.0, min(1.0, float(volume_text)))
    current_volume = -1.0

    def send_mci(command: str) -> int:
        return int(winmm.mciSendStringW(command, None, 0, None))

    def get_status(command: str) -> tuple[int, str]:
        status_buffer.value = ""
        error = int(winmm.mciSendStringW(command, status_buffer, len(status_buffer), None))
        return error, status_buffer.value.strip().lower()

    quoted_path = str(music_path.resolve()).replace('"', '""')
    open_error = send_mci(f'open "{quoted_path}" type mpegvideo alias {alias}')
    if open_error != 0:
        return open_error or 1
    try:
        play_error = send_mci(f'play {alias}')
        if play_error != 0:
            return play_error
        while True:
            if parent_pid and not is_windows_process_running(parent_pid):
                send_mci(f'stop {alias}')
                return 0
            state = load_startup_music_state(control_path)
            if state.get("stop"):
                send_mci(f'stop {alias}')
                return 0
            desired_volume = max(0.0, min(1.0, float(state.get("volume", base_volume))))
            if abs(desired_volume - current_volume) > 0.001:
                send_mci(f'setaudio {alias} volume to {int(desired_volume * 1000)}')
                current_volume = desired_volume
            status_error, status = get_status(f'status {alias} mode')
            if status_error != 0:
                return status_error
            if status not in {"playing", "seeking"}:
                if loop_playback:
                    send_mci(f'play {alias} from 0')
                else:
                    return 0
            time.sleep(0.2)
    finally:
        send_mci(f'close {alias}')


def resolve_audio_asset_path(file_names: tuple[str, ...]) -> Optional[Path]:
    candidates: list[Path] = []
    if getattr(sys, "frozen", False):
        executable_dir = Path(sys.executable).resolve().parent
        candidates.extend(executable_dir / "audio" / name for name in file_names)
        candidates.extend(executable_dir.parent / "audio" / name for name in file_names)
    source_dir = Path(__file__).resolve().parent
    repo_root = source_dir.parent.parent
    candidates.extend(repo_root / "auto_flash_tool" / "audio" / name for name in file_names)
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def play_prompt_audio(file_names: tuple[str, ...], fallback_sequence: tuple[tuple[int, int], ...]) -> None:
    audio_path = resolve_audio_asset_path(file_names)
    if audio_path is not None:
        play_audio_file(str(audio_path), str(PROMPT_SOUND_VOLUME))
        return
    play_tone_sequence(fallback_sequence)


def play_warning_prompt_audio() -> None:
    duck_startup_music()
    try:
        play_prompt_audio(WARNING_SOUND_FILENAMES, WARNING_TONES)
    finally:
        restore_startup_music_volume()


def play_tone_sequence(sequence: tuple[tuple[int, int], ...]) -> None:
    if os.name != "nt":
        return
    try:
        import winsound

        for frequency, duration_ms in sequence:
            if frequency <= 0:
                time.sleep(duration_ms / 1000.0)
            else:
                winsound.Beep(max(37, min(32767, frequency)), max(10, duration_ms))
            time.sleep(0.02)
    except Exception:
        try:
            import winsound

            winsound.MessageBeep(winsound.MB_ICONASTERISK)
        except Exception:
            pass


def play_hermesx_success_sound() -> None:
    play_prompt_audio(SUCCESS_SOUND_FILENAMES, HERMESX_SUCCESS_TONES)


def play_hermesx_failed_sound() -> None:
    play_prompt_audio(ERROR_SOUND_FILENAMES, HERMESX_FAILED_TONES)


def stop_startup_music() -> None:
    global STARTUP_MUSIC_PROCESS, STARTUP_MUSIC_CONTROL_PATH
    write_startup_music_state(stop=True)
    process = STARTUP_MUSIC_PROCESS
    if process is not None:
        try:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
        except Exception:
            pass
    STARTUP_MUSIC_PROCESS = None
    if STARTUP_MUSIC_CONTROL_PATH is not None:
        try:
            STARTUP_MUSIC_CONTROL_PATH.unlink(missing_ok=True)
        except Exception:
            pass
    STARTUP_MUSIC_CONTROL_PATH = None


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
    if tool_name == "play-music":
        if not tool_args:
            print("Missing music path.", file=sys.stderr)
            return 2
        volume_text = tool_args[1] if len(tool_args) > 1 else str(STARTUP_MUSIC_VOLUME)
        parent_pid_text = tool_args[2] if len(tool_args) > 2 else ""
        control_path_text = tool_args[3] if len(tool_args) > 3 else ""
        loop_text = tool_args[4] if len(tool_args) > 4 else "0"
        return play_audio_file(tool_args[0], volume_text, parent_pid_text, control_path_text, loop_text)

    print(f"Unknown internal tool: {tool_name}", file=sys.stderr)
    return 2


def pause_before_exit(message: str) -> None:
    print(message, flush=True)
    try:
        if os.name == "nt":
            import msvcrt

            msvcrt.getwch()
            return
    except Exception:
        pass
    if not sys.stdin or not sys.stdin.isatty():
        return
    try:
        input()
    except EOFError:
        pass


def run_detached_powershell(command: str) -> None:
    creation_flags = getattr(subprocess, "DETACHED_PROCESS", 0) | getattr(subprocess, "CREATE_NO_WINDOW", 0)
    subprocess.Popen(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Sta", "-WindowStyle", "Hidden", "-Command", command],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=creation_flags,
    )


def notify_user_attention(message: str, title: str = "HermesX AutoFlasher", success: bool = False) -> None:
    print(message, flush=True)
    stop_startup_music()
    if success:
        play_hermesx_success_sound()
    else:
        play_hermesx_failed_sound()
    if os.name != "nt":
        return
    try:
        ps_title = escape_powershell_single_quoted(title)
        ps_message = escape_powershell_single_quoted(message)
        run_detached_powershell(
            "Add-Type -AssemblyName PresentationFramework; "
            f"[System.Windows.MessageBox]::Show('{ps_message}', '{ps_title}', 'OK', 'Error') | Out-Null"
        )
    except Exception:
        pass


def requires_attention_for_subprocess_output(output: str) -> bool:
    if not output:
        return False
    patterns = (
        r"(?im)^traceback \(most recent call last\):",
        r"(?im)^\s*\[PYI-\d+:ERROR\]",
        r"(?i)\bfailed to execute script\b",
        r"(?i)\bmeshinterfaceerror\b",
        r"(?im)^\s*(?:FAIL|FAILED)\s*$",
    )
    return any(re.search(pattern, output) for pattern in patterns)


def summarize_subprocess_output(output: str) -> str:
    lines = [line.strip() for line in (output or "").splitlines() if line.strip()]
    if not lines:
        return ""
    for line in reversed(lines):
        if not re.match(r"(?i)^traceback \(most recent call last\):$", line):
            return line
    return lines[-1]


def pause_for_enter(message: str = "隢? Enter 蝜潛?...") -> None:
    print(message, flush=True)
    while True:
        try:
            input()
            return
        except EOFError:
            time.sleep(0.5)


def notify_and_pause_on_subprocess_failure(raw: str, output: str) -> None:
    summary = summarize_subprocess_output(output)
    message_lines = [
        "A subprocess reported a failure or traceback.",
        f"Command: {raw}",
    ]
    if summary:
        message_lines.append(f"Summary: {summary}")
    message_lines.append("Press Enter to continue.")
    notify_user_attention("\n".join(message_lines))
    pause_for_enter()


def escape_powershell_single_quoted(value: str) -> str:
    return value.replace("'", "''")


def escape_powershell_single_quoted(value: str) -> str:
    return value.replace("'", "''")


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
        parser = argparse.ArgumentParser(description="Meshtastic auto flash and config tool")
        parser.add_argument("--firmware-path", default="", help="Explicit firmware file path")
        parser.add_argument("--firmware-file-name", default="HermesX_0.2.8-beta0002-update.bin", help="Recorded firmware file name")
        parser.add_argument("--config-path", default="", help="Explicit config path or YAML generated from CLI.md")
        parser.add_argument("--config-file-name", default="config.yaml", help="Config file name")
        parser.add_argument("--cli-config-path", default="", help="Explicit CLI config path")
        parser.add_argument("--cli-config-file-name", default="CLI.md", help="CLI config file name")
        parser.add_argument("--export-config-yaml", default="", help="Write YAML converted from CLI.md to this path")
        parser.add_argument("--startup-music-path", default="", help="Startup MP3 path")
        parser.add_argument("--post-flash-wait-seconds", type=int, default=60, help="Seconds to wait after flashing")
        parser.add_argument("--reboot-batch-size", type=int, default=2, help="Number of commands per reboot batch")
        parser.add_argument("--reboot-wait-seconds", type=int, default=10, help="Seconds to wait after reboot")
        parser.add_argument("--reapply-max-passes", type=int, default=2, help="Maximum verification/reapply passes")
        parser.add_argument("--port-detect-timeout-seconds", type=int, default=60, help="Serial port detection timeout")
        parser.add_argument("--port-detect-interval-seconds", type=int, default=2, help="Serial port detection poll interval")
        parser.add_argument("--ready-timeout-seconds", type=int, default=30, help="Device ready-check timeout")
        parser.add_argument("--ready-poll-seconds", type=int, default=2, help="Device ready-check poll interval")
        parser.add_argument("--ready-command-timeout-seconds", type=int, default=10, help="Timeout for each ready-check command")
        parser.add_argument("--ready-retry-count", type=int, default=2, help="Ready-check retries per port")
        parser.add_argument("--meshtastic-timeout-seconds", type=int, default=120, help="meshtastic CLI timeout")
        parser.add_argument("--meshtastic-retry-count", type=int, default=3, help="meshtastic CLI retry count")
        parser.add_argument("--meshtastic-retry-delay-seconds", type=int, default=2, help="meshtastic CLI retry delay seconds")
        parser.add_argument("--log-path", default="", help="Log file path")
        parser.add_argument("--reboot-after-config", action="store_true", default=True, help="Reboot after applying config")
        parser.add_argument("--no-reboot-after-config", action="store_false", dest="reboot_after_config", help="Do not reboot after applying config")
        parser.add_argument("--post-config-reboot-wait-seconds", type=int, default=10, help="Seconds to wait after config reboot")
        parser.add_argument("--use-transaction", action="store_true", default=False, help="Use begin/commit edit transaction")
        return parser.parse_args()

    def _initialize_log(self) -> Path:
        default_log_dir = self.script_dir
        log_path = Path(self.args.log_path).expanduser() if self.args.log_path else default_log_dir / "flash_and_config.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(f"==== {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} ====\n", encoding="utf-8")
        LOGGER.setLevel(logging.INFO)
        LOGGER.handlers.clear()
        formatter = logging.Formatter("[%(asctime)s] %(message)s", datefmt="%H:%M:%S")
        console = SafeConsoleHandler(sys.stdout)
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
            self.log(f"霈????ASCII ?仃??{exc}", delay_after=False)


    def resolve_startup_music_path(self) -> Optional[Path]:
        candidates: list[Path] = []
        if self.args.startup_music_path:
            candidates.append(Path(self.args.startup_music_path).expanduser())
        candidates.extend(
            [
                self.script_dir / "audio" / "startup.mp3",
                self.script_dir / "audio" / "BIOS - Zorrovian (youtube).mp3",
                self.repo_root / "auto_flash_tool" / "audio" / "startup.mp3",
                self.repo_root / "auto_flash_tool" / "audio" / "BIOS - Zorrovian (youtube).mp3",
            ]
        )
        for candidate in candidates:
            if candidate.exists():
                return candidate.resolve()
        return None

    def play_startup_music(self) -> None:
        global STARTUP_MUSIC_PROCESS, STARTUP_MUSIC_CONTROL_PATH
        if os.name != "nt":
            return
        music_path = self.resolve_startup_music_path()
        if not music_path:
            return
        stop_startup_music()
        STARTUP_MUSIC_CONTROL_PATH = self.script_dir / "startup_music_state.json"
        write_startup_music_state(volume=STARTUP_MUSIC_VOLUME, stop=False)
        creation_flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        try:
            STARTUP_MUSIC_PROCESS = subprocess.Popen(
                self.build_internal_tool_command(
                    "play-music",
                    [str(music_path), str(STARTUP_MUSIC_VOLUME), str(os.getpid()), str(STARTUP_MUSIC_CONTROL_PATH), "1"],
                ),
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                creationflags=creation_flags,
            )
            self.log(f"Startup music launched: {music_path.name}", delay_after=False)
        except Exception as exc:
            STARTUP_MUSIC_PROCESS = None
            self.log(f"Startup music failed: {exc}", delay_after=False)

    @staticmethod
    def get_runtime_dir() -> Path:
        if getattr(sys, "frozen", False):
            return Path(sys.executable).resolve().parent
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
            raise FileNotFoundError(f"Target 鞈?憭曆?摮: {target_dir}")

        matches = sorted(target_dir.glob("*.bin"), key=lambda item: item.stat().st_mtime, reverse=True)
        if not matches:
            raise FileNotFoundError(f"Target 鞈?憭暹銝隞颱? .bin: {target_dir}")

        preferred_name = preferred_file_name.strip()
        if preferred_name and len(matches) > 1:
            self.log(
                f"preferred_file={preferred_name} is recorded only; firmware selection now always comes from Target, with a prompt when multiple .bin files exist."
            )

        if len(matches) == 1:
            return matches[0].resolve()

        self.log("Multiple firmware files were found in Target. Please choose one:")
        for index, item in enumerate(matches, start=1):
            self.log(f"[{index}] {item.name}")

        while True:
            choice = input(f"Select firmware file (1-{len(matches)}): ").strip()
            if choice.isdigit():
                selected_index = int(choice)
                if 1 <= selected_index <= len(matches):
                    return matches[selected_index - 1].resolve()
            print("Invalid selection. Please try again.")

    def resolve_cli_config_path(self) -> Path:
        if self.args.cli_config_path:
            candidate = Path(self.args.cli_config_path).expanduser()
            if candidate.exists():
                return candidate.resolve()
            raise FileNotFoundError(f"CLI config file not found: {candidate}")
        candidates = [
            self.script_dir / self.args.cli_config_file_name,
            self.repo_root / self.args.cli_config_file_name,
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate.resolve()
        raise FileNotFoundError(
            f"CLI config file was not found in runtime locations: {self.args.cli_config_file_name}"
        )

    def resolve_config_path(self) -> Path:
        if self.args.config_path:
            candidate = Path(self.args.config_path).expanduser()
            if candidate.exists():
                return candidate.resolve()
            raise FileNotFoundError(f"Config file not found: {candidate}")
        candidates = [
            self.script_dir / self.args.cli_config_file_name,
            self.repo_root / self.args.cli_config_file_name,
            self.script_dir / self.args.config_file_name,
            self.repo_root / self.args.config_file_name,
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate.resolve()
        raise FileNotFoundError(
            f"Config file was not found in runtime locations: {self.args.config_file_name} or {self.args.cli_config_file_name}"
        )

    def resolve_cli_source_path_from_config(self, config_path: Path, config: dict) -> Optional[Path]:
        source = config.get("source") or {}
        source_name = str(source.get("path") or "").strip()
        candidates: list[Path] = []
        if source_name:
            candidates.extend(
                [
                    (config_path.parent / source_name).resolve(),
                    (self.script_dir / source_name).resolve(),
                    (self.repo_root / source_name).resolve(),
                ]
            )
        candidates.extend(
            [
                (config_path.parent / self.args.cli_config_file_name).resolve(),
                (self.script_dir / self.args.cli_config_file_name).resolve(),
                (self.repo_root / self.args.cli_config_file_name).resolve(),
            ]
        )
        seen: set[Path] = set()
        for candidate in candidates:
            if candidate in seen:
                continue
            seen.add(candidate)
            if candidate.exists():
                return candidate
        return None

    @staticmethod
    def is_yaml_config(path: Path) -> bool:
        return path.suffix.lower() in {".yaml", ".yml"}

    @staticmethod
    def looks_like_yaml_config_text(text: str) -> bool:
        normalized_lines = [line.strip() for line in text.splitlines() if line.strip()]
        if not normalized_lines:
            return False
        yaml_markers = (
            "config_version:",
            "source:",
            "firmware:",
            "flash:",
            "channel_urls:",
            "channel_defaults:",
            "commands:",
        )
        return any(line.startswith(yaml_markers) for line in normalized_lines[:40])

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
            raise RuntimeError("YAML config must deserialize to an object/map")
        return data

    def get_serial_ports(self) -> list:
        return list(serial.tools.list_ports.comports())

    def select_serial_port(self, preferred_port: Optional[str] = None) -> str:
        ports = self.get_serial_ports()
        if not ports:
            raise RuntimeError("No serial ports were detected")
        non_legacy_ports = [
            port for port in ports
            if (port.device or "").upper() != "COM1"
            and "COMMUNICATIONS PORT" not in (port.description or "").upper()
        ]
        candidates = [
            port for port in non_legacy_ports
            if "VID:PID=303A" in (port.hwid or "").upper()
            or "USB" in (port.description or "").upper()
            or "USB" in port.device.upper()
        ]
        if preferred_port and any(port.device == preferred_port for port in candidates):
            return preferred_port
        if len(candidates) == 1:
            return candidates[0].device
        if len(candidates) > 1:
            self.log("?菜葫?啣???USB 摨???")
            for index, port in enumerate(candidates):
                self.log(f"[{index}] {port.device} ({port.description})")
            self.log("Multiple USB serial ports were detected; defaulting to the first candidate.")
            return candidates[0].device
        if preferred_port and any(port.device == preferred_port for port in non_legacy_ports):
            return preferred_port
        if len(non_legacy_ports) == 1:
            return non_legacy_ports[0].device
        if len(non_legacy_ports) > 1:
            return non_legacy_ports[0].device
        if preferred_port and any(port.device == preferred_port for port in ports):
            return preferred_port
        if len(ports) == 1:
            only_port = ports[0].device
            if only_port.upper() == "COM1":
                raise RuntimeError("Only legacy COM1 is available; waiting for the USB serial device")
            return only_port
        raise RuntimeError("Unable to determine which serial port to use")

    def wait_for_serial_port(self, preferred_port: Optional[str]) -> str:
        timeout = self.args.port_detect_timeout_seconds if self.args.port_detect_timeout_seconds > 0 else 60
        poll = self.args.port_detect_interval_seconds if self.args.port_detect_interval_seconds > 0 else 2
        deadline = time.time() + timeout
        warning_played = False
        while True:
            try:
                return self.select_serial_port(preferred_port)
            except RuntimeError as exc:
                if "No serial ports were detected" not in str(exc) and "Only legacy COM1 is available" not in str(exc):
                    raise
                if time.time() >= deadline:
                    raise RuntimeError(f"Timed out after {timeout} seconds waiting for a serial port") from exc
                if not warning_played:
                    play_warning_prompt_audio()
                    warning_played = True
                self.log(f"Waiting for a serial port, retrying in {poll} seconds...")
                time.sleep(poll)

    def wait_for_serial_port_with_user_reconnect(self, preferred_port: Optional[str], reason: str) -> str:
        while True:
            try:
                return self.wait_for_serial_port(preferred_port)
            except RuntimeError as exc:
                notify_user_attention(
                    "瘜冽?嚗?蝵桅???閬犖撌亥??n"
                    f"{reason}\n"
                    f"{exc}\n"
                    "Press Enter after reconnecting the device and serial cable."
                )
                try:
                    input()
                except EOFError:
                    time.sleep(2)

    def wait_for_operator_acknowledgement(self, reason: str) -> None:
        notify_user_attention(
            "瘜冽?嚗身摰神?亙歇?怠??n"
            f"{reason}\n"
            "Press Enter after confirming the device state."
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
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        output_lines: list[str] = []
        start = time.time()
        arg_text = " ".join(str(arg).lower() for arg in args)
        is_meshtastic_info = "meshtastic" in arg_text and "--info" in args
        while True:
            line = process.stdout.readline() if process.stdout else ""
            if line:
                text = line.rstrip("\n")
                output_lines.append(text)
                self.log(text, delay_after=False)
                if is_meshtastic_info and re.search(r"connected to radio", text, re.I):
                    process.kill()
                    try:
                        process.wait(timeout=1)
                    except subprocess.TimeoutExpired:
                        pass
                    return 0, "\n".join(output_lines)
            if process.poll() is not None:
                break
            if timeout and (time.time() - start) > timeout:
                process.kill()
                try:
                    process.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    pass
                partial_output = "\n".join(output_lines)
                raise subprocess.TimeoutExpired(args, timeout, output=partial_output)
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
                    if code != 0 and requires_attention_for_subprocess_output(output):
                        summary = summarize_subprocess_output(output) or f"exit code {code}"
                        self.log(f"Ready-check command failed transiently: {summary}", delay_after=False)
                    if re.search(r"connected to radio", output or "", re.I):
                        self.log("Device ready.")
                        return current_port
                except subprocess.TimeoutExpired as exc:
                    output = exc.output if isinstance(exc.output, str) else str(exc.output or "")
                    if re.search(r"connected to radio", output or "", re.I):
                        self.log("Device ready.")
                        return current_port
                    if requires_attention_for_subprocess_output(output):
                        summary = summarize_subprocess_output(output) or "timeout"
                        self.log(f"Ready-check command timed out or errored transiently: {summary}", delay_after=False)
                except Exception as exc:
                    output = str(exc)
                    if requires_attention_for_subprocess_output(output):
                        self.log(f"Ready-check command raised transient exception: {output}", delay_after=False)
                if time.time() < deadline:
                    self.log(f"Device not ready yet, retrying in {poll} seconds...")
                    time.sleep(poll)
            if attempt < tries - 1:
                play_warning_prompt_audio()
                self.log(f"Device still not ready after {timeout} seconds, re-detecting serial port...")
                self.log("Please reboot the device manually: press RESET, or reconnect USB if RESET is unavailable.")
                self.log("The tool will continue automatically after the device reappears.")
                current_port = self.wait_for_serial_port(current_port)
                self.log(f"Serial port after reconnect: {current_port}")
        self.log(f"Device still unresponsive after {timeout} seconds; continuing without waiting further.")
        return current_port

    def invoke_esptool_flash(self, port: str, firmware: Path) -> None:
        self.log(f"???瑕神????{port} ...")
        self.log("Please keep the device connected over USB during recovery.")
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
            raise RuntimeError(f"esptool failed with exit code {code}")

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
            raise RuntimeError("YAML commands section is missing a type field")
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
        if self.is_yaml_config(path) or self.looks_like_yaml_config_text(text):
            config = self.load_yaml_text(text)
            command_entries = config.get("commands") or []
            commands = [self.command_from_config_entry(entry) for entry in command_entries]
            if not commands:
                self.log(f"YAML config loaded but commands is empty: {path}", delay_after=False)
                cli_source_path = self.resolve_cli_source_path_from_config(path, config)
                if cli_source_path:
                    self.log(f"Falling back to CLI source: {cli_source_path}", delay_after=False)
                    fallback_config = self.build_config_from_cli_text(self.read_text_file_best_encoding(cli_source_path), cli_source_path.name)
                    fallback_entries = fallback_config.get("commands") or []
                    commands = [self.command_from_config_entry(entry) for entry in fallback_entries]
                    if fallback_entries:
                        config["commands"] = fallback_entries
                    if not (config.get("channel_urls") or {}):
                        config["channel_urls"] = fallback_config.get("channel_urls") or {}
                    if not (config.get("channel_defaults") or {}):
                        config["channel_defaults"] = fallback_config.get("channel_defaults") or {}
                    source = config.get("source") or {}
                    source["fallback_loaded_from"] = str(cli_source_path)
                    config["source"] = source
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
    def test_serialized_command(command: MeshtasticCommand) -> bool:
        if command.type == "SetCannedMessage":
            return True
        return command.type == "SetField" and bool(command.field) and command.field.lower().startswith("canned_message.")

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

    def invoke_meshtastic_commands(self, port: str, commands: list[MeshtasticCommand]) -> str:
        current_port = port
        reboot_commands = [command for command in commands if self.test_reboot_command(command)]
        remaining_commands = [command for command in commands if not self.test_reboot_command(command)]
        serialized_commands = [command for command in remaining_commands if self.test_serialized_command(command)]
        normal_commands = [command for command in remaining_commands if not self.test_serialized_command(command)]
        batch_size = max(1, self.args.reboot_batch_size)

        if reboot_commands:
            for batch_index, start in enumerate(range(0, len(reboot_commands), batch_size), start=1):
                batch = reboot_commands[start : start + batch_size]
                self.log(f"Applying reboot-sensitive batch {batch_index} with {len(batch)} commands...")
                for command in batch:
                    self.log(f"Command: {command.raw}")
                meshtastic_args = self.build_meshtastic_args(batch)
                self.log(f"meshtastic --port {current_port} {' '.join(shlex.quote(arg) for arg in meshtastic_args)}")
                current_port = self.invoke_meshtastic_with_retry(
                    current_port,
                    meshtastic_args,
                    "\n".join(command.raw for command in batch),
                    batch,
                    True,
                )
                if start + batch_size < len(reboot_commands) or normal_commands:
                    self.log(f"Waiting {self.args.reboot_wait_seconds} seconds for device reboot after batch {batch_index}...")
                    time.sleep(max(0, self.args.reboot_wait_seconds))
                    current_port = self.wait_for_serial_port(current_port)
                    self.log(f"Serial port after reboot batch: {current_port}")
                    current_port = self.wait_for_meshtastic_ready(current_port)

        if serialized_commands:
            self.log(f"Applying {len(serialized_commands)} serialized meshtastic commands...")
            for command_index, command in enumerate(serialized_commands, start=1):
                self.log(f"Applying serialized command {command_index}/{len(serialized_commands)}...")
                self.log(f"Command: {command.raw}")
                meshtastic_args = self.build_meshtastic_args([command])
                self.log(f"meshtastic --port {current_port} {' '.join(shlex.quote(arg) for arg in meshtastic_args)}")
                current_port = self.invoke_meshtastic_with_retry(
                    current_port,
                    meshtastic_args,
                    command.raw,
                    [command],
                    True,
                )
                self.log(f"Waiting {self.args.reboot_wait_seconds} seconds after serialized command...")
                time.sleep(max(0, self.args.reboot_wait_seconds))
                current_port = self.wait_for_serial_port(current_port)
                self.log(f"Serial port after serialized command: {current_port}")
                current_port = self.wait_for_meshtastic_ready(current_port)

        if normal_commands:
            needs_transaction = self.args.use_transaction and any(command.type in {"SetField", "SetCannedMessage"} for command in normal_commands)
            if needs_transaction:
                self.log("Opening meshtastic edit transaction...")
                current_port = self.invoke_meshtastic_with_retry(current_port, ["--begin-edit"], "meshtastic --begin-edit")
            self.log(f"Applying {len(normal_commands)} normal meshtastic commands...")
            for command in normal_commands:
                self.log(f"Command: {command.raw}")
            meshtastic_args = self.build_meshtastic_args(normal_commands)
            self.log(f"meshtastic --port {current_port} {' '.join(shlex.quote(arg) for arg in meshtastic_args)}")
            current_port = self.invoke_meshtastic_with_retry(
                current_port,
                meshtastic_args,
                "\n".join(command.raw for command in normal_commands),
                normal_commands,
                True,
            )
            if needs_transaction:
                self.log("Committing meshtastic edit transaction...")
                current_port = self.invoke_meshtastic_with_retry(current_port, ["--commit-edit"], "meshtastic --commit-edit")
        return current_port

    @staticmethod
    def convert_camel_to_snake(value: str) -> str:
        return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value).lower() if value else value

    def get_field_ack_variants(self, field: str) -> list[str]:
        parts = field.split('.')
        variants = {field}
        snake_parts = [self.convert_camel_to_snake(part) for part in parts]
        variants.add('.'.join(snake_parts))
        if len(parts) > 1:
            variants.add('.'.join([*parts[:-1], self.convert_camel_to_snake(parts[-1])]))
        normalized_variants = {self.normalize_info_path(variant) for variant in variants}
        return sorted(variants | normalized_variants)

    def get_command_ack_patterns(self, command: MeshtasticCommand) -> list[str]:
        if command.type == "SetField" and command.field:
            return [rf"(?i)\bset\s+{re.escape(field)}\s+to\b" for field in self.get_field_ack_variants(command.field)]
        if command.type == "SetCannedMessage":
            return [r"(?i)\bsetting\s+canned\s+plugin\s+message\b", r"(?i)\bset\s+canned\s+message\b", r"(?i)\bsetting\s+canned\s+message\b"]
        return []

    def test_output_for_commands(self, output: str, commands: list[MeshtasticCommand]) -> tuple[bool, list[str]]:
        missing: list[str] = []
        if not output:
            return False, ["meshtastic CLI returned no output"]
        if not re.search(r"connected to radio", output, re.I):
            missing.append("Missing expected output: Connected to radio")
        for command in commands:
            patterns = self.get_command_ack_patterns(command)
            if patterns and not any(re.search(pattern, output) for pattern in patterns):
                missing.append(f"Missing command acknowledgement: {command.raw}")
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
            except subprocess.TimeoutExpired as exc:
                code = 1
                output = exc.output if isinstance(exc.output, str) else str(exc.output or "")
            except Exception as exc:
                code, output = 1, str(exc)
            if code != 0 and requires_attention_for_subprocess_output(output):
                notify_and_pause_on_subprocess_failure(raw, output)
            if code == 0 or re.search(r"connected to radio", output or "", re.I):
                return output
            if attempt < self.args.meshtastic_retry_count:
                delay = max(0, self.args.meshtastic_retry_delay_seconds)
                if delay > 0:
                    self.log(f"meshtastic command failed {attempt}/{self.args.meshtastic_retry_count}; retrying in {delay} seconds...")
                    time.sleep(delay)
                else:
                    self.log(f"meshtastic command failed {attempt}/{self.args.meshtastic_retry_count}; retrying immediately...")
                continue
            raise RuntimeError(f"meshtastic command failed: {raw}\n{output}")
        raise RuntimeError(f"meshtastic command failed: {raw}")

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
                except subprocess.TimeoutExpired as exc:
                    code = 1
                    output = exc.output if isinstance(exc.output, str) else str(exc.output or "")
                except Exception as exc:
                    code, output = 1, str(exc)

                last_output = output
                if code != 0 and requires_attention_for_subprocess_output(output):
                    notify_and_pause_on_subprocess_failure(raw, output)

                success = code == 0 or bool(re.search(r"connected to radio", output or "", re.I))
                if success and require_ack:
                    success, missing = self.test_output_for_commands(output, expected_commands or [])
                    if not success and missing:
                        self.log("Missing expected meshtastic output:\n" + "\n".join(missing))
                if success:
                    return current_port

                manual_reconnect = self.test_manual_reconnect_required(output)
                connection_issue = self.test_connection_issue(output)
                if attempt < retry_count:
                    if manual_reconnect:
                        self.log("The serial port requires manual reconnect before retrying.")
                        current_port = self.wait_for_serial_port_with_user_reconnect(
                            current_port,
                            "Reconnect the device, then continue the ready-check.",
                        )
                        self.log(f"Port reconnected: {current_port}")
                        current_port = self.wait_for_meshtastic_ready(current_port)
                        continue
                    if connection_issue:
                        self.log("The device disconnected during configuration; waiting for it to come back.")
                        current_port = self.wait_for_serial_port_with_user_reconnect(
                            current_port,
                            "Reconnect or reboot the device, then continue configuration.",
                        )
                        self.log(f"Port reconnected: {current_port}")
                        current_port = self.wait_for_meshtastic_ready(current_port)
                        continue
                    delay = max(0, self.args.meshtastic_retry_delay_seconds)
                    if delay > 0:
                        self.log(f"meshtastic command failed on attempt {attempt}/{retry_count}; retrying in {delay} seconds...")
                        time.sleep(delay)
                    else:
                        self.log(f"meshtastic command failed on attempt {attempt}/{retry_count}; retrying immediately...")
                    continue

            self.wait_for_operator_acknowledgement(
                "The device is still not ready after retries. Check power, cable, and reboot state before continuing."
            )
            current_port = self.wait_for_serial_port_with_user_reconnect(
                current_port,
                "Press Enter after the device is ready and the serial port is available.",
            )
            self.log(f"Port reconnected: {current_port}")
            current_port = self.wait_for_meshtastic_ready(current_port)
            if last_output:
                self.log("Last meshtastic output:")
                self.log(last_output, delay_after=False)

    @staticmethod
    def normalize_info_value(value: Optional[str]) -> str:
        normalized = "" if value is None else str(value).strip().strip('"').rstrip(",")
        lowered = normalized.lower()
        if lowered in {"true", "false"}:
            return lowered
        if lowered in {"on", "off"}:
            return "true" if lowered == "on" else "false"
        try:
            return f"num:{float(normalized):.15g}"
        except ValueError:
            return normalized

    @staticmethod
    def values_match_for_field(field: Optional[str], actual: Optional[str], expected: Optional[str]) -> bool:
        actual_normalized = MeshtasticAutoFlash.normalize_info_value(actual)
        expected_normalized = MeshtasticAutoFlash.normalize_info_value(expected)
        if actual_normalized == expected_normalized:
            return True
        normalized_field = MeshtasticAutoFlash.normalize_info_path(field or "")
        enum_aliases = {
            "lora.modem_preset": {
                "num:4": "MEDIUM_FAST",
            },
            "lora.region": {
                "num:8": "TW",
            },
            "position.gps_mode": {
                "num:1": "ENABLED",
            },
            "canned_message.inputbroker_event_cw": {
                "num:17": "UP",
            },
            "canned_message.inputbroker_event_ccw": {
                "num:18": "DOWN",
            },
            "canned_message.inputbroker_event_press": {
                "num:10": "SELECT",
            },
        }
        aliases = enum_aliases.get(normalized_field)
        if not aliases:
            return False
        expected_text = "" if expected is None else str(expected).strip()
        actual_text = "" if actual is None else str(actual).strip()
        return aliases.get(actual_normalized) == expected_text or aliases.get(expected_normalized) == actual_text or aliases.get(actual_text) == expected_text

    def convert_info_to_map(self, output: str) -> dict[str, str]:
        info_map: dict[str, str] = {}
        stack: list[str] = []
        for raw_line in (output or "").splitlines():
            line = raw_line.rstrip()
            if not line or re.match(r"(?i)^connected to radio$", line.strip()):
                continue
            indent = len(line) - len(line.lstrip())
            level = indent // 2
            stripped = line.strip().rstrip(',')
            while len(stack) > level:
                stack.pop()
            match = re.match(r'^"?([A-Za-z0-9_ ]+)"?\s*:\s*(.*)$', stripped)
            if not match:
                continue
            key = self.normalize_info_path(match.group(1).replace(' ', '_'))
            value = match.group(2).strip()
            if value in {"", "{", "["}:
                stack.append(key)
                continue
            path = ".".join([*stack, key])
            info_map[path] = self.normalize_info_value(value)
        return info_map

    @staticmethod
    def normalize_info_path(path: str) -> str:
        path = path.strip().strip('"')
        if path.startswith("Preferences."):
            path = path[len("Preferences."):]
        if path.startswith("Module preferences."):
            path = path[len("Module preferences."):]
        if path.startswith("cannedMessage."):
            path = "canned_message." + path[len("cannedMessage."):]
        parts = [part.replace(' ', '_') for part in path.split('.') if part]
        normalized_parts = [re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', part).lower() for part in parts]
        return '.'.join(normalized_parts)

    def get_info_channel_url(self, output: str) -> Optional[str]:
        for line in (output or "").splitlines():
            m = re.search(r'(?i)^primary channel url:\s*(\S+)', line.strip())
            if m:
                return m.group(1).strip()
            m = re.search(r'(?i)^complete url[^:]*:\s*(\S+)', line.strip())
            if m:
                return m.group(1).strip()
        return None

    def get_expected_canned_message(self, commands: list[MeshtasticCommand]) -> Optional[str]:
        messages = [command.message for command in commands if command.type == "SetCannedMessage" and command.message]
        return messages[-1] if messages else None

    def get_canned_message_from_device(self, port: str) -> str:
        output = self.invoke_meshtastic_capture(port, ["--get-canned-message"], "meshtastic --get-canned-message")
        match = re.search(r'(?im)^canned_plugin_message\s*:\s*(.+)$', output or "")
        if match:
            return match.group(1).strip().strip('"')
        lines = [line.strip() for line in (output or "").splitlines() if line.strip() and not re.match(r'(?i)^connected to radio$', line.strip())]
        return lines[-1] if lines else ""

    def get_meshtastic_info(self, port: str) -> str:
        return self.invoke_meshtastic_capture(port, ["--info"], "meshtastic --info")

    def get_field_value_from_device(self, port: str, field: str) -> tuple[Optional[str], str]:
        raw = f"meshtastic --get {field}"
        output = ""
        current_port = port
        for attempt in range(2):
            try:
                output = self.invoke_meshtastic_capture(current_port, ["--get", field], raw)
                break
            except Exception as exc:
                message = str(exc)
                if attempt == 0 and self.test_connection_issue(message):
                    self.log(f"Precheck read failed for {field}; waiting for device reconnect before retrying...")
                    try:
                        current_port = self.wait_for_serial_port(current_port)
                        current_port = self.wait_for_meshtastic_ready(current_port)
                    except Exception:
                        pass
                    continue
                self.log(f"Precheck could not read {field}; treating it as changed.")
                return None, current_port
        if not output or re.search(r"timed out waiting for connection completion", output, re.I):
            try:
                current_port = self.wait_for_meshtastic_ready(current_port)
            except Exception:
                pass
            try:
                output = self.invoke_meshtastic_capture(current_port, ["--get", field], raw)
            except Exception:
                self.log(f"Precheck could not read {field} after ready-check retry; treating it as changed.")
                return None, current_port
        variants = [self.normalize_info_path(variant) for variant in self.get_field_ack_variants(field)]
        fallback_value: Optional[str] = None
        for raw_line in (output or "").splitlines():
            line = raw_line.strip().rstrip(",")
            if not line or re.match(r"(?i)^connected to radio$", line):
                continue
            match = re.match(r'^"?([A-Za-z0-9_. ]+)"?\s*:\s*(.*)$', line)
            if match:
                key = self.normalize_info_path(match.group(1).replace(" ", "_"))
                value = self.normalize_info_value(match.group(2).strip())
                if key in variants:
                    return value, current_port
                fallback_value = value
        return fallback_value, current_port

    def get_channel_url_command_if_mismatch(self, expected_channel_url: Optional[str], actual_channel_url: Optional[str]) -> Optional[MeshtasticCommand]:
        expected = (expected_channel_url or "").strip()
        actual = (actual_channel_url or "").strip()
        if not expected:
            return None
        if not actual:
            return None
        if actual and (expected == actual or expected in actual or actual in expected):
            return None
        return MeshtasticCommand(type="SetChannelUrl", url=expected, raw=f"meshtastic --ch-set-url {expected}")

    def get_actual_info_value(self, info_map: dict[str, str], field: str) -> Optional[str]:
        for variant in self.get_field_ack_variants(field):
            normalized_variant = self.normalize_info_path(variant)
            if normalized_variant in info_map:
                return info_map[normalized_variant]
        return None

    def get_missing_commands(self, commands: list[MeshtasticCommand], info_map: dict[str, str]) -> tuple[list[MeshtasticCommand], list[MeshtasticCommand]]:
        missing: list[MeshtasticCommand] = []
        unverified: list[MeshtasticCommand] = []
        for command in commands:
            if command.type == "SetField" and command.field:
                actual = self.get_actual_info_value(info_map, command.field)
                if actual is None:
                    unverified.append(command)
                elif not self.values_match_for_field(command.field, actual, command.value):
                    missing.append(command)
            elif command.type == "AddChannelUrl":
                unverified.append(command)
        return missing, unverified

    def filter_commands_against_device(
        self,
        port: str,
        commands: list[MeshtasticCommand],
        expected_channel_url: Optional[str],
    ) -> list[MeshtasticCommand]:
        current_port = port
        info_text = self.get_meshtastic_info(current_port)
        actual_channel_url = self.get_info_channel_url(info_text)
        actual_canned_message: Optional[str] = None
        field_value_cache: dict[str, Optional[str]] = {}
        filtered: list[MeshtasticCommand] = []

        for command in commands:
            if command.type == "SetField" and command.field:
                if command.field not in field_value_cache:
                    actual_value, current_port = self.get_field_value_from_device(current_port, command.field)
                    field_value_cache[command.field] = actual_value
                actual = field_value_cache[command.field]
                if self.values_match_for_field(command.field, actual, command.value):
                    self.log(f"Skipping unchanged field: {command.field}={command.value}")
                    continue
                filtered.append(command)
                continue

            if command.type == "SetChannelUrl":
                expected_url = (command.url or "").strip()
                if expected_url and actual_channel_url and (expected_url == actual_channel_url or expected_url in actual_channel_url or actual_channel_url in expected_url):
                    self.log("Skipping unchanged primary channel URL")
                    continue
                filtered.append(command)
                continue

            if command.type == "SetCannedMessage":
                if actual_canned_message is None:
                    actual_canned_message = self.get_canned_message_from_device(current_port)
                expected_message = (command.message or "").strip()
                if expected_message == (actual_canned_message or ""):
                    self.log("Skipping unchanged canned message")
                    continue
                filtered.append(command)
                continue

            filtered.append(command)

        self.log(f"Diff precheck kept {len(filtered)} of {len(commands)} commands.")
        return filtered

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
            self.log(f"Verifying device settings, pass {current_pass}/{max_passes}...")
            info_text = self.get_meshtastic_info(current_port)
            info_map = self.convert_info_to_map(info_text)
            missing, unverified = self.get_missing_commands(commands, info_map)
            actual_channel_url = self.get_info_channel_url(info_text)
            channel_command = self.get_channel_url_command_if_mismatch(expected_channel_url, actual_channel_url)
            if not missing and not channel_command:
                self.log("Configuration verification passed.")
                if unverified:
                    self.log(f"{len(unverified)} commands could not be confirmed from --info output.")
                verified = True
                break
            for command in missing:
                actual = info_map.get(command.field or "", "<missing from --info>")
                self.log(f"Config mismatch: {command.raw}; expected {command.value}; actual {actual}")
            if channel_command:
                self.log(f"Channel URL mismatch: {channel_command.raw}")
            if unverified:
                self.log(f"{len(unverified)} commands could not be confirmed from --info output.")
            reapply = list(missing)
            if channel_command and actual_channel_url:
                reapply.append(channel_command)
                reapply.extend(channel_default_commands)
            if reapply and current_pass < max_passes:
                self.log(f"Reapplying {len(reapply)} commands before the next verification pass...")
                current_port = self.invoke_meshtastic_commands(current_port, reapply)
                time.sleep(2)
        if not verified:
            raise RuntimeError(f"Configuration verification failed after {max_passes} passes")
        return current_port

    def verify_canned_message(self, port: str, commands: list[MeshtasticCommand]) -> str:
        expected = self.get_expected_canned_message(commands)
        if not expected:
            return port
        current_port = port
        max_passes = self.args.reapply_max_passes if self.args.reapply_max_passes > 0 else 1
        for current_pass in range(1, max_passes + 1):
            self.log(f"Verifying canned message, pass {current_pass}/{max_passes}...")
            actual = self.get_canned_message_from_device(current_port)
            if actual == expected:
                self.log("Canned message verification passed.")
                return current_port
            self.log(f'Canned message mismatch: expected "{expected}"; actual "{actual or "<empty>"}"')
            if current_pass < max_passes:
                command = MeshtasticCommand(type="SetCannedMessage", message=expected, raw=f"meshtastic --set-canned-message {expected}")
                self.log("Reapplying canned message...")
                current_port = self.invoke_meshtastic_with_retry(current_port, ["--set-canned-message", expected], command.raw, [command], True)
                time.sleep(2)
        raise RuntimeError(f"Canned message verification failed after {max_passes} passes")

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
        self.play_startup_music()
        self.print_startup_banner()
        self.ensure_dependencies()

        if self.args.export_config_yaml:
            cli_path = self.resolve_cli_config_path()
            output_path = Path(self.args.export_config_yaml).expanduser()
            self.export_cli_to_yaml(cli_path, output_path)
            self.log(f"Exported YAML config to {output_path}")
            return

        config_path = self.resolve_config_path()
        commands, expected_channel_url, channel_default_commands, config = self.load_runtime_config(config_path)
        if (
            not self.args.config_path
            and self.is_yaml_config(config_path)
            and not commands
        ):
            fallback_cli_path = self.resolve_cli_config_path()
            if fallback_cli_path != config_path:
                self.log(
                    f"偵測到 YAML 設定檔沒有任何 meshtastic 命令，改用舊版 CLI 設定檔：{fallback_cli_path}"
                )
                config_path = fallback_cli_path
                commands, expected_channel_url, channel_default_commands, config = self.load_runtime_config(config_path)
        commands = self.insert_channel_commands_after_url(commands, channel_default_commands)
        if not commands:
            raise RuntimeError(f"No meshtastic commands were found in config: {config_path}")

        firmware_name = ((config.get("firmware") or {}).get("preferred_file") or "").strip()
        firmware_path = self.resolve_firmware_path(firmware_name)
        self.log("=== Meshtastic auto flash started ===")
        self.log(f"Firmware: {firmware_path}")
        self.log(f"Config: {config_path}")
        if firmware_name and firmware_name != firmware_path.name:
            self.log(f"Config recorded preferred_file={firmware_name}, but selected firmware is {firmware_path.name}")

        initial_port = self.select_serial_port()
        self.log(f"Initial serial port: {initial_port}")
        self.log("Firmware flashing will start now. Keep USB connected and avoid interrupting the device.")

        self.invoke_esptool_flash(initial_port, firmware_path)
        play_warning_prompt_audio()
        self.log(f"Flash completed. Waiting {self.args.post_flash_wait_seconds} seconds before ready-check...")
        self.log("Do not disconnect the device while waiting for it to reboot and reconnect.")
        time.sleep(max(0, self.args.post_flash_wait_seconds))

        port_after = self.wait_for_serial_port(initial_port)
        self.log(f"Serial port after flash: {port_after}")
        port_after = self.wait_for_meshtastic_ready(port_after)
        commands_to_apply = self.filter_commands_against_device(port_after, commands, expected_channel_url)
        if commands_to_apply:
            self.log(f"Applying {len(commands_to_apply)} meshtastic commands after the device becomes ready.")
            port_after = self.invoke_meshtastic_commands(port_after, commands_to_apply)
        else:
            self.log("Device already matches config. Skipping meshtastic apply stage.")

        if self.args.reboot_after_config:
            self.log("Configuration applied. Rebooting device for final verification.")
            port_after = self.invoke_meshtastic_with_retry(port_after, ["--reboot"], "meshtastic --reboot")
            self.log(f"Waiting {self.args.post_config_reboot_wait_seconds} seconds after config reboot...")
            time.sleep(max(0, self.args.post_config_reboot_wait_seconds))
            port_after = self.wait_for_serial_port(port_after)
            self.log(f"Serial port after config reboot: {port_after}")
            port_after = self.wait_for_meshtastic_ready(port_after)

        port_after = self.verify_device_settings(port_after, commands, expected_channel_url, channel_default_commands)
        port_after = self.verify_canned_message(port_after, commands)

        self.log("=== Meshtastic auto flash completed ===")
        self.log("Process finished. Review the log if you need to troubleshoot this device later.")

def main() -> int:
    configure_stdio_for_unicode()
    set_windows_console_green()
    helper_exit_code = try_run_internal_helper(sys.argv[1:])
    if helper_exit_code is not None:
        return helper_exit_code

    app: Optional[MeshtasticAutoFlash] = None
    log_path: Optional[Path] = None
    try:
        app = MeshtasticAutoFlash()
        log_path = app.log_path
        app.run()
        stop_startup_music()
        play_hermesx_success_sound()
        pause_before_exit("\u8a2d\u5b9a\u5b8c\u7562!\u73fe\u5728\u4f60\u53ef\u4ee5\u958b\u59cb\u4f60\u7684\u65c5\u7a0b\u4e86!\n\u6309\u4efb\u610f\u9375\u96e2\u958b")
        return 0
    except KeyboardInterrupt:
        LOGGER.info("\u4f7f\u7528\u8005\u4e2d\u65b7\u57f7\u884c")
        notify_user_attention("\u57f7\u884c\u5df2\u88ab\u4f7f\u7528\u8005\u4e2d\u65b7")
        pause_before_exit("\u5df2\u4e2d\u6b62\uff0c\u6309\u4efb\u610f\u9375\u9000\u51fa")
        return 130
    except Exception as exc:
        LOGGER.error(str(exc))
        notify_user_attention(f"\u57f7\u884c\u5931\u6557\uff1a{exc}")
        if log_path is None and app is not None:
            log_path = app.log_path
        if log_path is not None:
            pause_before_exit(
                f"\u5931\u6557\uff0c\u8acb\u67e5\u770b log \u4e26\u63d0\u4ea4\u7d66\u5718\u968a\u505a\u78ba\u8a8d\nlog \u6a94\u4f4d\u7f6e\uff1a{log_path}\n\u6309\u4efb\u610f\u9375\u9000\u51fa"
            )
        else:
            pause_before_exit("\u5931\u6557\uff0c\u8acb\u67e5\u770b log \u4e26\u63d0\u4ea4\u7d66\u5718\u968a\u505a\u78ba\u8a8d\uff0c\u6309\u4efb\u610f\u9375\u9000\u51fa")
        return 1

if __name__ == "__main__":
    raise SystemExit(main())


