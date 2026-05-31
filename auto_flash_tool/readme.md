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
