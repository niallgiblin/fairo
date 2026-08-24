#!/usr/bin/env bash
#
# Build the Fairo VST3 with ONNX Runtime and install it for REAPER on macOS
# (ad-hoc signed, ONNX dylib bundled inside the .vst3 so nothing breaks after
# a reboot or when the machine's temp dir is cleared).
#
# Usage:  bash scripts/install_macos.sh            (uses the local JUCE checkout if present)
#         bash scripts/install_macos.sh --fresh    (delete build-onnx and reconfigure)
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORT_VERSION="1.29.0"
ORT_DIR="${ORT_DIR:-$HOME/.cache/fairo/onnxruntime-osx-arm64-$ORT_VERSION}"
INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

# 1. ONNX Runtime present? (download once, kept under ~/.cache so it survives reboots)
if [ ! -f "$ORT_DIR/lib/libonnxruntime.dylib" ]; then
    mkdir -p "$(dirname "$ORT_DIR")"
    case "$(uname -m)" in
        arm64) ORT_ARCH="arm64" ;;
        x86_64) ORT_ARCH="x86_64" ;;
        *) echo "unsupported arch: $(uname -m)" && exit 1 ;;
    esac
    echo ">>> downloading ONNX Runtime $ORT_VERSION ..."
    curl -sL "https://github.com/microsoft/onnxruntime/releases/download/v$ORT_VERSION/onnxruntime-osx-$ORT_ARCH-$ORT_VERSION.tgz" \
        -o /tmp/onnxruntime-fairo.tgz
    tar xzf /tmp/onnxruntime-fairo.tgz -C "$(dirname "$ORT_DIR")"
    rm -f /tmp/onnxruntime-fairo.tgz
fi

# 2. Configure + build (reuse the cached JUCE checkout from fuzzyband if available)
JUCE_DIR="$HOME/Desktop/fuzzyband/build-release/_deps/juce-src"
if [ -d "$JUCE_DIR" ]; then
    JUCE_ARGS="-DFETCHCONTENT_SOURCE_DIR_JUCE=$JUCE_DIR"
else
    JUCE_ARGS=()
fi

echo ">>> configuring (FA_ENABLE_ONNX=ON) ..."
cmake -B "$ROOT/build-onnx" -DCMAKE_BUILD_TYPE=Release \
    "${JUCE_ARGS[@]}" \
    -DFA_ENABLE_ONNX=ON \
    -DONNXRUNTIME_ROOT="$ORT_DIR"

echo ">>> building ..."
cmake --build "$ROOT/build-onnx" --config Release --parallel "$(sysctl -n hw.ncpu)"

# 3. Bundle the ONNX dylib inside the .vst3 and fix @rpath, then ad-hoc sign
VST3="$ROOT/build-onnx/Fairo_artefacts/Release/VST3/Fairo.vst3"
BIN="$VST3/Contents/MacOS/Fairo"

cp "$ORT_DIR/lib/libonnxruntime.1.dylib" "$VST3/Contents/MacOS/"
# Drop any stale rpath pointing at the ORT build dir; resolve via @loader_path.
for RP in $(otool -l "$BIN" | awk '/LC_RPATH/{f=1} f && / path /{print $2; f=0}'); do
    if [ "$RP" != "@loader_path" ]; then
        install_name_tool -delete_rpath "$RP" "$BIN" 2>/dev/null || true
    fi
done
if ! otool -l "$BIN" | grep -q "@loader_path"; then
    install_name_tool -add_rpath @loader_path "$BIN" 2>/dev/null || true
fi

echo ">>> codesigning (ad-hoc) ..."
codesign --force --deep -s - "$VST3"

# 4. Install into the user VST3 folder
mkdir -p "$INSTALL_DIR"
rm -rf "$INSTALL_DIR/Fairo.vst3"
cp -R "$VST3" "$INSTALL_DIR/Fairo.vst3"
codesign --verify --deep "$INSTALL_DIR/Fairo.vst3" && echo "signature OK"

echo ""
echo ">>> installed: $INSTALL_DIR/Fairo.vst3"
echo ">>> In REAPER: Preferences -> Plug-ins -> VST -> Re-scan, or use the"
echo "               \"Rescan\" button in the FX browser."
echo ">>> The plugin shows as \"Fairo\" (FX category: Distortion)."
