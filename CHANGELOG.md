# Changelog

## 2026-03-20

### AutoFlasher

- Added YAML-first configuration support for the auto flasher via [`auto_flash_tool/config.yaml`](/Users/oldways/HermesX/auto_flash_tool/config.yaml).
- Kept backward compatibility with legacy [`auto_flash_tool/CLI.md`](/Users/oldways/HermesX/auto_flash_tool/CLI.md) and added CLI-to-YAML export support in [`tooling/auto_flash_builder/auto_installer.py`](/Users/oldways/HermesX/tooling/auto_flash_builder/auto_installer.py).
- Synced the current `CLI.md` settings into the new checked-in YAML config.
- Improved flashing flow so the preferred firmware in YAML is auto-selected when present, reducing manual prompts.
- Added a protective retry mode: when device settings fail to write repeatedly, the tool now pauses, emits an audible alert, asks the user to replug the device, and then continues retrying instead of exiting immediately with failure.
- Applied the protective retry flow in shared Python logic so it affects both Windows and macOS builds.
- Added macOS launcher support through [`auto_flash_tool/Launch_Meshtastic_Auto_Flash.command`](/Users/oldways/HermesX/auto_flash_tool/Launch_Meshtastic_Auto_Flash.command).
- Expanded macOS packaging in [`tooling/auto_flash_builder/platform/macos/build.sh`](/Users/oldways/HermesX/tooling/auto_flash_builder/platform/macos/build.sh) so distributable output includes runtime files, config, firmware folders, and launcher scripts.
- Updated AutoFlasher documentation to mention `config.yaml` as the primary config source and `CLI.md` as fallback/migration input.
