#!/usr/bin/env bash
set -euo pipefail

PYTHON_EXE="${PYTHON_EXE:-python3}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDER_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
REPO_ROOT="$(cd "${BUILDER_ROOT}/../.." && pwd)"
RUNTIME_ROOT="${REPO_ROOT}/auto_flash_tool"
SPEC_PATH="${BUILDER_ROOT}/platform/shared/meshtastic_auto_flash.spec"
BUILD_ROOT="${BUILDER_ROOT}/build/macos"
DIST_ROOT="${BUILDER_ROOT}/dist/macos"
BUNDLE_ROOT="${DIST_ROOT}/Meshtastic_Auto_Flash"
PUBLISH_ROOT="${RUNTIME_ROOT}/tool_macos"
PACKAGE_ROOT="${DIST_ROOT}/auto_flash_tool"

echo "Building standalone macOS bundle..."
"${PYTHON_EXE}" -m PyInstaller --noconfirm --clean --workpath "${BUILD_ROOT}" --distpath "${DIST_ROOT}" "${SPEC_PATH}"

rm -rf "${PUBLISH_ROOT}"
mkdir -p "${PUBLISH_ROOT}"
cp -R "${BUNDLE_ROOT}/." "${PUBLISH_ROOT}/"

cat > "${PUBLISH_ROOT}/Launch_Meshtastic_Auto_Flash.command" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "${SCRIPT_DIR}/Meshtastic_Auto_Flash"
EOF
chmod +x "${PUBLISH_ROOT}/Launch_Meshtastic_Auto_Flash.command"

cat > "${RUNTIME_ROOT}/Launch_Meshtastic_Auto_Flash.command" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "${SCRIPT_DIR}/tool_macos/Meshtastic_Auto_Flash"
EOF
chmod +x "${RUNTIME_ROOT}/Launch_Meshtastic_Auto_Flash.command"

rm -rf "${PACKAGE_ROOT}"
mkdir -p "${PACKAGE_ROOT}"
cp -R "${RUNTIME_ROOT}/Target" "${PACKAGE_ROOT}/Target"
cp -R "${RUNTIME_ROOT}/release" "${PACKAGE_ROOT}/release"
cp "${RUNTIME_ROOT}/readme.md" "${PACKAGE_ROOT}/readme.md"
cp "${RUNTIME_ROOT}/CLI.md" "${PACKAGE_ROOT}/CLI.md"
cp "${RUNTIME_ROOT}/config.yaml" "${PACKAGE_ROOT}/config.yaml"
cp "${RUNTIME_ROOT}/Launch_Meshtastic_Auto_Flash.command" "${PACKAGE_ROOT}/Launch_Meshtastic_Auto_Flash.command"
cp "${REPO_ROOT}/ascii-art-text-1773857730689.txt" "${PACKAGE_ROOT}/ascii-art-text-1773857730689.txt"
cp -R "${PUBLISH_ROOT}" "${PACKAGE_ROOT}/tool_macos"

echo "macOS runtime synced to: ${PUBLISH_ROOT}"
echo "macOS distributable package ready at: ${PACKAGE_ROOT}"
