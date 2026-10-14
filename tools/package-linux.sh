#!/usr/bin/env bash
# Builds the Linux x86_64 Release and packages dist/GeometryWarsRetroEvolved2-v<version>-linux-x86_64.AppImage.
# Local only: nothing is published, tagged or uploaded. Run on Linux or WSL.
#
#   tools/package-linux.sh            incremental Release build, then package
#   tools/package-linux.sh --clean    clean Release build first
#
# Needs clang-20, clang++-20, cmake 3.25+, ninja, rsync, wget or curl, unzip or python3, binutils and the
# GCC 13+ C++ runtime. Release builds are made on Ubuntu 22.04: the AppImage needs the build machine's glibc.
# Needs generated/default (tools/codegen.ps1) and game/extracted/default.xex: the build re-runs codegen
# when its inputs look changed, and it never packages either of them.
# The ReXGlue runtime and GPU plugin are built from source with the project's patches
# (tools/build-patched-rexruntime.sh) the first time, which also needs network access and the packages in
# docs/rexglue-runtime-patch.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# On the Linux filesystem: much faster than a Windows drive under /mnt, and case-sensitive.
WORK="${GW2_WORK:-$HOME/gw2-linux-build}"
PRESET=linux-amd64-release

SDK_VERSION=0.10.0
SDK_SHA256=0388edb3f8fb1444f1b0ad5d440cb02d9690ac20a76b04fd6b7dcc19f59328a6
XEX_SHA256=6d5d9980a1d17dc74b9ccf540cec16c2e144e7cee3a74938e78c237b8aceeb88
APPIMAGETOOL_VERSION=1.9.1
APPIMAGETOOL_SHA256=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
RUNTIME_VERSION=20251108
RUNTIME_SHA256=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d

CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

fail() { echo "ERROR: $*" >&2; exit 1; }

download() {  # url file sha256
  if [ ! -f "$2" ] || ! echo "$3  $2" | sha256sum -c --status -; then
    rm -f "$2"
    if command -v wget >/dev/null; then wget -q -O "$2" "$1"; else curl -fsSL -o "$2" "$1"; fi
  fi
  echo "$3  $2" | sha256sum -c --status - || fail "$2 does not match its pinned SHA-256"
}

VERSION="$(sed -nE 's/^[[:space:]]*project\([[:space:]]*geometry_wars_retro_evolved_2[[:space:]]+VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$VERSION" ] || fail "project VERSION not found in CMakeLists.txt"
APP_ID="$(sed -nE 's/^[[:space:]]*constexpr char kApplicationId\[\] = "([^"]+)";.*/\1/p' "$ROOT/src/launcher_platform_linux.cpp")"
[[ "$APP_ID" =~ ^[A-Za-z0-9_-]+(\.[A-Za-z0-9_-]+)+$ ]] ||
  fail "src/launcher_platform_linux.cpp must declare kApplicationId exactly once, as a dotted name on one line (found: '$APP_ID')"
[ -f "$ROOT/packaging/linux/$APP_ID.desktop" ] || fail "packaging/linux/$APP_ID.desktop is missing: the desktop file is named after kApplicationId"
NAME="GeometryWarsRetroEvolved2-v$VERSION-linux-x86_64"
echo "Geometry Wars: Retro Evolved 2 $VERSION - Linux AppImage (local only)"

echo "== Tools"
for tool in clang-20 clang++-20 cmake ninja rsync sha256sum strip readelf objdump ldd; do
  command -v "$tool" >/dev/null || fail "$tool not found"
done
command -v wget >/dev/null || command -v curl >/dev/null || fail "wget or curl not found"
command -v unzip >/dev/null || command -v python3 >/dev/null || fail "unzip or python3 not found"
clang++-20 --version | sed -n 1p
cmake --version | sed -n 1p
HOST_GLIBC="$(getconf GNU_LIBC_VERSION | cut -d' ' -f2)"
echo "glibc $HOST_GLIBC"
[ "$(printf '%s\n' 2.35 "$HOST_GLIBC" | sort -V | tail -1)" = 2.35 ] ||
  echo "WARNING: this AppImage will need glibc $HOST_GLIBC; release builds use Ubuntu 22.04 (glibc 2.35)" >&2

echo "== Inputs"
[ -f "$ROOT/generated/default/sources.cmake" ] || fail "generated/default is missing (run tools/codegen.ps1 first)"
[ -f "$ROOT/third_party/trg-launcher/CMakeLists.txt" ] || fail "run: git submodule update --init third_party/trg-launcher"
XEX="$ROOT/game/extracted/default.xex"
[ -f "$XEX" ] || fail "missing $XEX (extract your own XBLA package into game/extracted first)"
echo "$XEX_SHA256  $XEX" | sha256sum -c --status - || fail "unsupported $XEX"

echo "== Source -> $WORK"
mkdir -p "$WORK/game/extracted" "$WORK/third_party" "$WORK/tools"
for f in CMakeLists.txt CMakePresets.json geometry_wars_retro_evolved_2_manifest.toml LICENSE tools/build-patched-rexruntime.sh; do
  rsync -a "$ROOT/$f" "$WORK/$f"
done
for d in config src tests generated packaging patches/rexglue-sdk third_party/trg-launcher; do
  mkdir -p "$WORK/$d"
  rsync -a --delete --exclude .git "$ROOT/$d/" "$WORK/$d/"
done
rsync -a "$XEX" "$WORK/game/extracted/default.xex"

echo "== ReXGlue SDK $SDK_VERSION (linux-amd64)"
SDK="$WORK/tools/rexglue/linux-amd64"
if [ ! -x "$SDK/bin/rexglue" ]; then
  mkdir -p "$WORK/tools/rexglue"
  ZIP="$WORK/tools/rexglue/rexglue-sdk-$SDK_VERSION-linux-amd64.zip"
  download "https://github.com/rexglue/rexglue-sdk/releases/download/v$SDK_VERSION/rexglue-sdk-$SDK_VERSION-linux-amd64.zip" \
    "$ZIP" "$SDK_SHA256"
  rm -rf "$SDK"
  if command -v unzip >/dev/null; then
    unzip -q -o "$ZIP" -d "$WORK/tools/rexglue"
  else
    python3 -I -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' "$ZIP" "$WORK/tools/rexglue"
    chmod +x "$SDK/bin/rexglue"
  fi
fi
[ "$("$SDK/bin/rexglue" --version | tr -d '[:space:]')" = "$SDK_VERSION" ] || fail "tools/rexglue/linux-amd64 is not ReXGlue $SDK_VERSION"

echo "== Patched ReXGlue runtime and GPU plugin"
PATCHED="$ROOT/tools/build-patched-rexruntime.sh"
if ! bash "$PATCHED" --check "$SDK" --config Release >/dev/null 2>&1; then
  bash "$PATCHED" "$SDK" "$WORK/rexglue-sdk-src" --config Release
fi
bash "$PATCHED" --check "$SDK" --config Release

echo "== Build ($PRESET)"
BIN="$WORK/out/build/$PRESET"
[ "$CLEAN" = 1 ] && rm -rf "$BIN"
(cd "$WORK" && cmake --preset "$PRESET" >/dev/null && cmake --build --preset "$PRESET")
for f in geometry_wars_retro_evolved_2 librexruntime.so librexgpu-xenos.so geometry_wars_retro_evolved_2.toml; do
  [ -f "$BIN/$f" ] || fail "build output is missing $f"
done
bash "$PATCHED" --check "$SDK" --config Release --staged "$BIN" || fail "the build folder does not hold the patched ReXGlue libraries"

echo "== AppDir"
APPDIR="$WORK/AppDir"
DOC="$APPDIR/usr/share/doc/geometry_wars_retro_evolved_2"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$DOC"
cp "$BIN/geometry_wars_retro_evolved_2" "$BIN/librexruntime.so" "$BIN/librexgpu-xenos.so" \
  "$BIN/geometry_wars_retro_evolved_2.toml" "$APPDIR/usr/bin/"
# Debug sections carry the build machine's folders.
strip --strip-unneeded "$APPDIR/usr/bin/geometry_wars_retro_evolved_2"
# ReXGlue needs GLIBCXX_3.4.32 (GCC 13); AppRun uses this copy only on systems without it.
for lib in libstdc++.so.6 libgcc_s.so.1; do
  src="$(g++ -print-file-name=$lib 2>/dev/null || true)"
  [ -f "$src" ] || src="/usr/lib/x86_64-linux-gnu/$lib"
  cp -L "$src" "$APPDIR/usr/lib/$lib"
done
grep -q GLIBCXX_3.4.32 "$APPDIR/usr/lib/libstdc++.so.6" || fail "the build machine's libstdc++ is older than GCC 13"
cp "$WORK/LICENSE" "$WORK/packaging/licenses/ReXGlue-LICENSE.txt" "$WORK/packaging/licenses/THIRD-PARTY-LICENSES.txt" \
  "$WORK/packaging/licenses/THIRD-PARTY-LICENSES-LINUX.txt" "$DOC/"
CXX_COPYRIGHT=/usr/share/doc/libstdc++6/copyright
[ -f "$CXX_COPYRIGHT" ] || fail "missing $CXX_COPYRIGHT (the license of the bundled C++ runtime)"
cp "$CXX_COPYRIGHT" "$DOC/libstdc++-copyright.txt"
cp "$WORK/packaging/linux/AppRun" "$WORK/packaging/linux/$APP_ID.desktop" "$APPDIR/"
# The icon is the root file the .desktop names, the .DirIcon file managers show, and the theme the desktop integrates.
ICONS="$WORK/packaging/icons"
THEME="$APPDIR/usr/share/icons/hicolor"
for size in 16 24 32 48 64 128 256 512; do
  mkdir -p "$THEME/${size}x${size}/apps"
  cp "$ICONS/app-icon-$size.png" "$THEME/${size}x${size}/apps/geometry_wars_retro_evolved_2.png"
done
mkdir -p "$THEME/scalable/apps"
cp "$ICONS/app-icon.svg" "$THEME/scalable/apps/geometry_wars_retro_evolved_2.svg"
cp "$ICONS/app-icon-256.png" "$APPDIR/geometry_wars_retro_evolved_2.png"
cp "$ICONS/app-icon-256.png" "$APPDIR/.DirIcon"
chmod 755 "$APPDIR/AppRun" "$APPDIR/usr/bin/geometry_wars_retro_evolved_2"
chmod 644 "$APPDIR/"*.desktop "$APPDIR/"*.png "$APPDIR/.DirIcon" "$APPDIR/usr/bin/"*.so \
  "$APPDIR/usr/bin/geometry_wars_retro_evolved_2.toml" "$APPDIR/usr/lib/"* "$DOC/"*
find "$THEME" -type f -exec chmod 644 {} +
bash "$PATCHED" --check "$SDK" --config Release --staged "$APPDIR/usr/bin" || fail "the AppDir does not hold the patched ReXGlue libraries"

echo "== Release check"
problems=0
problem() { echo "  $*" >&2; problems=$((problems + 1)); }
while IFS= read -r -d '' item; do
  rel="${item#"$APPDIR"/}"
  base="$(basename "$item")"
  case "$base" in
    default.xex | Data.bin | game | game-install-temp | game-install-backup | user_data | user_data.migrating | content | cache | logs | saves | \
      settings.toml | settings.toml.bak | ArcadeInfo.xml | nxeart | 584108FF) problem "forbidden name: $rel" ;;
    *.xex | *.log | *.dmp | *.pdb | *.xsh | *.xpso | *.bak | *.tmp | *.sav) problem "forbidden file type: $rel" ;;
  esac
  case "${base,,}" in
    *renderdoc* | *vklayer* | *.json | implicit_layer.d | explicit_layer.d) problem "RenderDoc or Vulkan layer file: $rel" ;;
  esac
  [ -L "$item" ] && problem "symbolic link: $rel"
  if [ -f "$item" ]; then
    case "$(head -c 4 "$item" | tr -d '\0')" in LIVE | PIRS | "CON ") problem "Xbox 360 package: $rel" ;; esac
    for path in "$ROOT" "$WORK" "$HOME"; do
      grep -qaF "$path" "$item" && problem "local path in file: $rel ($path)"
    done
    grep -qaE '[A-Za-z]:[\\/](Users|VisualStudio|Decomp)|/mnt/[a-z]/|gw2-linux-build' "$item" &&
      problem "local path in file: $rel (developer folder)"
    [ "$(head -c 2 "$item")" = MZ ] && problem "Windows binary: $rel"
  fi
done < <(find "$APPDIR" -mindepth 1 -print0)
EXPECTED=".DirIcon
AppRun
geometry_wars_retro_evolved_2.png
$APP_ID.desktop
usr/bin/geometry_wars_retro_evolved_2
usr/bin/geometry_wars_retro_evolved_2.toml
usr/bin/librexgpu-xenos.so
usr/bin/librexruntime.so
usr/lib/libgcc_s.so.1
usr/lib/libstdc++.so.6
usr/share/doc/geometry_wars_retro_evolved_2/LICENSE
usr/share/doc/geometry_wars_retro_evolved_2/ReXGlue-LICENSE.txt
usr/share/doc/geometry_wars_retro_evolved_2/THIRD-PARTY-LICENSES-LINUX.txt
usr/share/doc/geometry_wars_retro_evolved_2/THIRD-PARTY-LICENSES.txt
usr/share/doc/geometry_wars_retro_evolved_2/libstdc++-copyright.txt
usr/share/icons/hicolor/128x128/apps/geometry_wars_retro_evolved_2.png
usr/share/icons/hicolor/16x16/apps/geometry_wars_retro_evolved_2.png
usr/share/icons/hicolor/24x24/apps/geometry_wars_retro_evolved_2.png
usr/share/icons/hicolor/256x256/apps/geometry_wars_retro_evolved_2.png
usr/share/icons/hicolor/32x32/apps/geometry_wars_retro_evolved_2.png
usr/share/icons/hicolor/48x48/apps/geometry_wars_retro_evolved_2.png
usr/share/icons/hicolor/512x512/apps/geometry_wars_retro_evolved_2.png
usr/share/icons/hicolor/64x64/apps/geometry_wars_retro_evolved_2.png
usr/share/icons/hicolor/scalable/apps/geometry_wars_retro_evolved_2.svg"
ACTUAL="$(cd "$APPDIR" && find . ! -type d | sed 's|^\./||' | LC_ALL=C sort)"
[ "$ACTUAL" = "$EXPECTED" ] || { problem "unexpected AppDir contents:"; diff <(echo "$EXPECTED") <(echo "$ACTUAL") >&2 || true; }
grep -qxF "StartupWMClass=$APP_ID" "$APPDIR/$APP_ID.desktop" || problem "StartupWMClass in $APP_ID.desktop is not $APP_ID"
grep -qaF "$APP_ID" "$APPDIR/usr/bin/geometry_wars_retro_evolved_2" || problem "the program does not contain the application id $APP_ID"
for f in geometry_wars_retro_evolved_2 librexruntime.so librexgpu-xenos.so; do
  dynamic="$(readelf -d "$APPDIR/usr/bin/$f")"
  if grep -qi renderdoc <<< "$dynamic"; then problem "$f depends on RenderDoc"; fi
done
readelf -d "$APPDIR/usr/bin/geometry_wars_retro_evolved_2" | grep -q 'R.*PATH.*\[\$ORIGIN\]' || problem "the program's RPATH is not \$ORIGIN"
missing="$(ldd "$APPDIR/usr/bin/geometry_wars_retro_evolved_2" | grep 'not found' || true)"
[ -z "$missing" ] || problem "unresolved libraries: $missing"
[ "$problems" = 0 ] || fail "the AppDir holds files that must not be released"
echo "  clean"
echo "  RPATH: $(readelf -d "$APPDIR/usr/bin/geometry_wars_retro_evolved_2" | grep -E 'R(UN)?PATH' | sed -E 's/.*\[(.*)\]/\1/')"
echo "  needed: $(readelf -d "$APPDIR/usr/bin/geometry_wars_retro_evolved_2" | grep NEEDED | sed -E 's/.*\[(.*)\]/\1/' | tr '\n' ' ')"
for f in bin/geometry_wars_retro_evolved_2 bin/librexruntime.so bin/librexgpu-xenos.so lib/libstdc++.so.6 lib/libgcc_s.so.1; do
  glibc="$(objdump -T "$APPDIR/usr/$f" | grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -1)"
  echo "  $(basename "$f") needs $glibc"
done

echo "== AppImage"
TOOLS="$WORK/tools/appimage"
mkdir -p "$TOOLS"
TOOL="$TOOLS/appimagetool-$APPIMAGETOOL_VERSION-x86_64.AppImage"
RUNTIME="$TOOLS/runtime-$RUNTIME_VERSION-x86_64"
download "https://github.com/AppImage/appimagetool/releases/download/$APPIMAGETOOL_VERSION/appimagetool-x86_64.AppImage" \
  "$TOOL" "$APPIMAGETOOL_SHA256"
download "https://github.com/AppImage/type2-runtime/releases/download/$RUNTIME_VERSION/runtime-x86_64" \
  "$RUNTIME" "$RUNTIME_SHA256"
chmod +x "$TOOL"
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/$NAME.AppImage"
rm -f "$OUT"
ARCH=x86_64 "$TOOL" --appimage-extract-and-run --no-appstream --runtime-file "$RUNTIME" "$APPDIR" "$OUT" >/dev/null
chmod 755 "$OUT"
(cd "$ROOT/dist" && sha256sum "$NAME.AppImage" > "$NAME.sha256")

echo
echo "===================================================="
echo " APPIMAGE READY (nothing was published)"
echo "===================================================="
echo "  AppImage: $OUT ($(du -h "$OUT" | cut -f1))"
echo "  SHA-256:  $(cut -d' ' -f1 "$ROOT/dist/$NAME.sha256")"
echo "  Contents:"
echo "$EXPECTED" | sed 's/^/    /'
