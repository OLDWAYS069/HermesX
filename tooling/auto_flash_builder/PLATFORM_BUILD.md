# Platform Build Notes

## Layout

- Shared runtime: `auto_installer.py`, `config.yaml`, `CLI.md`, `Target/`
- Windows packaging: `platform/windows/build.ps1`
- macOS packaging: `platform/macos/build.sh`
- Shared PyInstaller spec: `platform/shared/meshtastic_auto_flash.spec`
- Build outputs: `dist/windows/` and `dist/macos/`

## Goal

The packaged application is intended to be self-contained for end users:

- No preinstalled Python required
- No preinstalled `meshtastic` CLI required
- No preinstalled `esptool` required

## Developer Expectation

You still need to build on each target platform:

- Build Windows output on Windows
- Build macOS output on macOS

One shared source tree can serve both platforms, but the final bundled artifact is platform-specific.

## Config Format

- `config.yaml` is the primary config format.
- `CLI.md` is kept for backward compatibility and migration.
- To regenerate YAML from the legacy file:

```bash
python3 tooling/auto_flash_builder/auto_installer.py --export-config-yaml auto_flash_tool/config.yaml
```
