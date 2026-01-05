#!/usr/bin/env python3
"""HermesX EM Chinese font generator."""
from __future__ import annotations

import argparse
import platform
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path
from typing import Iterable, List

try:
    import urllib.request
except ImportError as exc:  # pragma: no cover
    raise SystemExit(f"urllib not available: {exc}") from exc


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools" / "fonts"
BIN_DIR = TOOLS_DIR / "bin"
CACHE_DIR = TOOLS_DIR / "cache"
OUT_DIR = TOOLS_DIR / "generated"

CHARSET_PATH = TOOLS_DIR / "hermesx_em_charset.txt"
BDF_PATH = CACHE_DIR / "unifont.bdf"
RAW_FONT_PATH = OUT_DIR / "HermesX_EM16_ZH.c"
CPP_PATH = REPO_ROOT / "src" / "graphics" / "fonts" / "OLEDDisplayFontsZH.cpp"
H_PATH = REPO_ROOT / "src" / "graphics" / "fonts" / "OLEDDisplayFontsZH.h"

BDF_URL = "https://raw.githubusercontent.com/olikraus/u8g2/master/tools/font/bdf/unifont.bdf"
BDFCONV_URL_WIN = "https://raw.githubusercontent.com/olikraus/u8g2/master/tools/font/bdfconv/bdfconv.exe"

FONT_SYMBOL = "HermesX_EM16_ZH"


def info(message: str) -> None:
    print(f"[hermesx-font] {message}")


def ensure_directories() -> None:
    for path in (BIN_DIR, CACHE_DIR, OUT_DIR):
        path.mkdir(parents=True, exist_ok=True)


def ensure_charset() -> None:
    if not CHARSET_PATH.exists():
        raise SystemExit(f"Charset file missing: {CHARSET_PATH}")


def download(url: str, dest: Path) -> None:
    info(f"Downloading {url} -> {dest}")
    with urllib.request.urlopen(url) as response, dest.open("wb") as fh:
        shutil.copyfileobj(response, fh)


def ensure_bdf() -> None:
    if BDF_PATH.exists():
        return
    ensure_directories()
    download(BDF_URL, BDF_PATH)


def ensure_bdfconv() -> Path:
    binary = shutil.which("bdfconv")
    if binary:
        return Path(binary)

    exe_name = "bdfconv.exe" if platform.system() == "Windows" else "bdfconv"
    candidate = BIN_DIR / exe_name
    if candidate.exists():
        return candidate

    if platform.system() == "Windows":
        ensure_directories()
        download(BDFCONV_URL_WIN, candidate)
        return candidate

    raise SystemExit(
        textwrap.dedent(
            """
            bdfconv not found. Install u8g2 tooling first:

              git clone https://github.com/olikraus/u8g2.git
              cd u8g2/tools/font/bdfconv && make
              export PATH=$PWD:$PATH
            """
        ).strip()
    )


def read_charset() -> List[str]:
    ensure_charset()
    seen: List[str] = []
    for line in CHARSET_PATH.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        for ch in line:
            if ch not in seen:
                seen.append(ch)
    return seen


def format_map(chars: Iterable[str], base: int = 0x80) -> str:
    parts = ["32-126"]  # keep ASCII at native codepoints
    offset = 0
    for ch in chars:
        code = ord(ch)
        if 32 <= code <= 126:
            continue  # ASCII stays mapped to itself
        parts.append(f"${code:04x}>${base + offset:02x}")
        offset += 1
    return ",".join(parts)


def run_bdfconv(bdfconv: Path, mapping: str, verbose: bool) -> None:
    ensure_bdf()
    ensure_directories()

    cmd = [
        str(bdfconv),
        "-f",
        "1",
        "-m",
        mapping,
        "-n",
        FONT_SYMBOL,
        "-o",
        str(RAW_FONT_PATH),
        str(BDF_PATH),
    ]
    if verbose:
        cmd.insert(1, "-v")

    info("Running bdfconv...")
    subprocess.run(cmd, check=True)


def ensure_header() -> None:
    if H_PATH.exists():
        return
    info(f"Writing header {H_PATH}")
    H_PATH.write_text(
        textwrap.dedent(
            """
            #ifndef OLEDDISPLAYFONTSZH_h
            #define OLEDDISPLAYFONTSZH_h

            #ifdef ARDUINO
            #include <Arduino.h>
            #elif __MBED__
            #define PROGMEM
            #endif

            extern const uint8_t HermesX_EM16_ZH[] PROGMEM;

            #endif
            """
        ).strip() + "\n",
        encoding="utf-8",
    )


def transform_cpp() -> None:
    raw_text = RAW_FONT_PATH.read_text(encoding="utf-8")
    transformed = raw_text.replace('U8G2_FONT_SECTION("HermesX_EM16_ZH")', "PROGMEM")

    if '#include "OLEDDisplayFontsZH.h"' not in transformed:
        transformed = '#include "OLEDDisplayFontsZH.h"\n\n' + transformed

    info(f"Updating {CPP_PATH}")
    CPP_PATH.write_text(transformed, encoding="utf-8")


def summarize(chars: List[str]) -> None:
    size_bytes = CPP_PATH.stat().st_size
    info(f"Glyphs included: {len(chars)}")
    info(f"Font resource size: {size_bytes / 1024:.2f} KB ({size_bytes} bytes)")
    info("Mapping:")
    base = 0x80
    for offset, ch in enumerate(chars):
        codepoint = ord(ch)
        info(f"  U+{codepoint:04X} -> 0x{base + offset:02X} ({ch})")


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose bdfconv output")
    args = parser.parse_args(argv)

    ensure_directories()
    ensure_header()

    chars = read_charset()
    mapping = format_map(chars)

    bdfconv_path = ensure_bdfconv()
    run_bdfconv(bdfconv_path, mapping, verbose=args.verbose)
    transform_cpp()
    summarize(chars)
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main(sys.argv[1:]))
