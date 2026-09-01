#!/usr/bin/env bash
#
# Build the Fairo VST3 (DSP-only) and install it for REAPER on macOS
# (ad-hoc signed).
#
# Usage:  bash scripts/install_macos.sh            (uses the local JUCE checkout if present)
#         bash scripts/install_macos.sh --fresh    (delete build and reconfigure)
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

if [ "${1:-}" = "--fresh" ]; then
    rm -rf "$BUILD_DIR"
fi

# Reuse the cached JUCE checkout from fuzzyband if available
JUCE_DIR="$HOME/Desktop/fuzzyband/build-release/_deps/juce-src"
if [ -d "$JUCE_DIR" ]; then
    JUCE_ARGS="-DFETCHCONTENT_SOURCE_DIR_JUCE=$JUCE_DIR"
else
    JUCE_ARGS=()
fi

echo ">>> configuring ..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    "${JUCE_ARGS[@]}"

echo ">>> building ..."
cmake --build "$BUILD_DIR" --config Release --parallel "$(sysctl -n hw.ncpu)"

VST3="$BUILD_DIR/Fairo_artefacts/Release/VST3/Fairo.vst3"

# Gatekeeper: a quarantined, ad-hoc-signed bundle can be refused by the OS.
xattr -cr "$VST3" 2>/dev/null || true

echo ">>> codesigning (ad-hoc) ..."
codesign --force --deep -s - "$VST3"

mkdir -p "$INSTALL_DIR"
rm -rf "$INSTALL_DIR/Fairo.vst3"
cp -R "$VST3" "$INSTALL_DIR/Fairo.vst3"
xattr -cr "$INSTALL_DIR/Fairo.vst3" 2>/dev/null || true
codesign --verify --deep "$INSTALL_DIR/Fairo.vst3" && echo "signature OK"

echo ""
echo ">>> installed: $INSTALL_DIR/Fairo.vst3"
echo ">>> In REAPER: Preferences -> Plug-ins -> VST -> Re-scan, or use the"
echo "               \"Rescan\" button in the FX browser."
echo ">>> The plugin shows as \"Fairo\" (FX category: Distortion)."
