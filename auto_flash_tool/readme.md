# HermesX Auto Flasher

## Canonical Windows Release

Distribute only the published Windows release folder or zip:

- `HermesX_UPDATER/`
- `HermesX_UPDATER.zip`

Do not distribute the repo root, `tooling/`, `.venv/`, `.pio/`, or the duplicate source-side runtime files.

## Release Layout

The end-user Windows package must contain:

- `HermesX_UPDATER.exe`
- `_internal/`
- `Target/`
- `config.yaml`
- `CLI.md`
- `audio/` if startup / prompt sounds are desired

The executable expects these items to stay in the same folder layout. Renaming the outer folder is OK. Renaming the EXE is usually OK, but the release process publishes `HermesX_UPDATER.exe` as the canonical default.

## Build Output

Build the Windows package with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tooling\auto_flash_builder\platform\windows\build.ps1 -PythonExe C:\Users\OLDDWAYS\AppData\Local\Programs\Python\Python313\python.exe
```

That build publishes:

- `auto_flash_tool/HermesX_UPDATER/`
- `auto_flash_tool/HermesX_UPDATER.zip`

## Source vs Release

The outer `auto_flash_tool/` directory is the source-side asset root used by the build.
End users should run only the published release folder.

## Path Safety Guidance

For the most reliable distribution:

- Prefer ASCII folder names for the published package.
- Prefer ASCII EXE and firmware filenames.
- Keep `_internal/`, `Target/`, `config.yaml`, and `CLI.md` next to the EXE.
- Avoid moving files out of the published folder.

## WiFi Handoff Update Route

The updater also has a HermesX handoff OTA route for firmware built by the main
HermesX tree.

Start normally to choose an update route:

```powershell
.\HermesX_UPDATER.exe
```

Routes:

- `[1] USB 燒錄 + 設定`
- `[2] 本機 WiFi 更新`
- `[3] 線上抓取 CIV 韌體並更新`

Local WiFi update uses the same firmware lookup behavior as the USB flasher:
put `.bin` files in `Target/`, then choose the file when prompted.
The updater asks for the device IP before uploading; press Enter to use the
configured default.

```powershell
.\HermesX_UPDATER.exe --mode wifi-update
```

Online CIV update downloads the configured Google Drive firmware into `Target/`,
then uploads it to `http://192.168.43.21/upload-update-bin` with the required
`X-Hermes-Filename` header.

```powershell
.\HermesX_UPDATER.exe --mode online-civ
```

To skip the IP prompt:

```powershell
.\HermesX_UPDATER.exe --mode online-civ --wifi-device-ip 192.168.43.21
```
