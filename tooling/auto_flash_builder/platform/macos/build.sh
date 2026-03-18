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

echo "Building standalone macOS bundle..."
"${PYTHON_EXE}" -m PyInstaller --noconfirm --clean --workpath "${BUILD_ROOT}" --distpath "${DIST_ROOT}" "${SPEC_PATH}"

rm -rf "${PUBLISH_ROOT}"
mkdir -p "${PUBLISH_ROOT}"
cp -R "${BUNDLE_ROOT}/." "${PUBLISH_ROOT}/"
rm -f "${PUBLISH_ROOT}/CLI.md" "${PUBLISH_ROOT}/README.md" "${PUBLISH_ROOT}/ascii-art-text-1773857730689.txt"
rm -rf "${PUBLISH_ROOT}/Target"

echo "macOS bundle ready at: ${PUBLISH_ROOT}"
