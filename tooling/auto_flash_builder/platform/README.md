# Platform Build Layout

This folder separates platform-specific packaging work from the shared flashing logic.

- `shared/`
  Shared PyInstaller spec and packaging settings.
- `windows/`
  Windows build script. Produces a self-contained folder with `Meshtastic_Auto_Flash.exe`.
- `macos/`
  macOS build script. Produces a self-contained app bundle or executable folder on macOS.

End-user runtime files stay next to the built application:

- `config.yaml`
- `CLI.md`
- `Target/`
- `README.md`
- `ascii-art-text-1773857730689.txt`

The goal is:

1. Developers build once per platform.
2. End users do not need to install Python, `meshtastic`, or `esptool`.
3. New builds should prefer `config.yaml`; `CLI.md` remains as a fallback and migration source.
