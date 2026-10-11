#!/usr/bin/env bash
# Build TheBigs2-x86_64.AppImage from a finished Linux release build.
#
#   packaging/build-appimage.sh            # uses out/build/linux-amd64-release
#   BUILD_DIR=... packaging/build-appimage.sh
#
# The AppImage contains the recompiled executable and the ReXGlue runtime, but
# no game data: on first launch it asks for your disc image (.iso).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/out/build/linux-amd64-release}"
OUT_DIR="${OUT_DIR:-$ROOT/out/appimage}"
APPDIR="$OUT_DIR/AppDir"
TOOL="$OUT_DIR/appimagetool-x86_64.AppImage"
TOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"

[ -x "$BUILD_DIR/thebigs2" ] || { echo "No build at $BUILD_DIR - build the linux-amd64-release preset first." >&2; exit 1; }

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/scalable/apps"

# Executable + everything it loads from its own folder ($ORIGIN rpath):
# librexruntime.so, libTracyClient.so and the GPU plugin librexgpu-xenos.so.
cp "$BUILD_DIR/thebigs2" "$APPDIR/usr/bin/"
cp "$BUILD_DIR"/*.so "$APPDIR/usr/bin/"
[ -f "$BUILD_DIR/gamecontrollerdb.txt" ] && cp "$BUILD_DIR/gamecontrollerdb.txt" "$APPDIR/usr/bin/"
if [ "${STRIP:-1}" = 1 ] && command -v strip >/dev/null; then
  strip --strip-unneeded "$APPDIR/usr/bin/thebigs2" "$APPDIR"/usr/bin/*.so
fi

cp "$ROOT/packaging/AppRun" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"
cp "$ROOT/packaging/thebigs2.desktop" "$APPDIR/"
cp "$ROOT/packaging/thebigs2.desktop" "$APPDIR/usr/share/applications/"
cp "$ROOT/packaging/thebigs2.svg" "$APPDIR/"
cp "$ROOT/packaging/thebigs2.svg" "$APPDIR/usr/share/icons/hicolor/scalable/apps/"
ln -sf thebigs2.svg "$APPDIR/.DirIcon"

if [ ! -x "$TOOL" ]; then
  echo "Downloading appimagetool..."
  curl -fL --retry 3 -o "$TOOL" "$TOOL_URL"
  chmod +x "$TOOL"
fi

# Extract-and-run: works without FUSE (e.g. in containers / CI).
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$TOOL" --no-appstream "$APPDIR" "$OUT_DIR/TheBigs2-x86_64.AppImage"
echo
echo "Built $OUT_DIR/TheBigs2-x86_64.AppImage"
