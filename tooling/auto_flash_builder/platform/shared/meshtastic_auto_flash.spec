# -*- mode: python ; coding: utf-8 -*-

from pathlib import Path

from PyInstaller.utils.hooks import collect_all


spec_dir = Path(SPECPATH).resolve()
tool_root = spec_dir.parents[1]
app_name = "Meshtastic_Auto_Flash"

datas = []
binaries = []
hiddenimports = []

for package_name in (
    "meshtastic",
    "esptool",
    "serial",
    "pubsub",
    "yaml",
    "google.protobuf",
    "pygame",
    "requests",
    "certifi",
):
    pkg_datas, pkg_binaries, pkg_hiddenimports = collect_all(package_name)
    datas += pkg_datas
    binaries += pkg_binaries
    hiddenimports += pkg_hiddenimports

a = Analysis(
    [str(tool_root / "auto_installer.py")],
    pathex=[str(tool_root)],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)
pyz = PYZ(a.pure)
exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name=app_name,
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name=app_name,
)
