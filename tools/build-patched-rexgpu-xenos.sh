#!/usr/bin/env bash
# Builds ReXGlue's Xenos GPU plugin (librexgpu-xenos.dylib) from the pinned v0.10.0 source with
# patches/rexglue-sdk/0002-spirv-texture-exp-adjust.patch and installs it into the extracted mac-arm64 SDK, where the
# game's build picks it up. Without the patch the Vulkan renderer ignores the texture exponent bias and the game is
# far too dark (Linux too; rexglue-sdk#334, #456).
#
#   tools/build-patched-rexgpu-xenos.sh [SDK_DIR] [WORK_DIR]   build and install (network needed the first time)
#   tools/build-patched-rexgpu-xenos.sh --check [SDK_DIR]      only verify that SDK_DIR holds the patched plugin
#   tools/build-patched-rexgpu-xenos.sh --restore [SDK_DIR]    put the stock plugin back
#
# SDK_DIR defaults to tools/rexglue/mac-arm64 (tools/codegen.sh unpacks it), WORK_DIR to out/rexglue-sdk-src.
# Needs git, clang++ (Command Line Tools), cmake 3.27+ and ninja. Rebuild the game afterwards so it copies the plugin.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PATCH_DIR="$ROOT/patches/rexglue-sdk"
PATCH="$PATCH_DIR/0002-spirv-texture-exp-adjust.patch"
STAMP_NAME=rexgpu-xenos-patch.txt
LIB=lib/librexgpu-xenos.dylib
STOCK=lib/librexgpu-xenos.stock.dylib

# shellcheck source=../patches/rexglue-sdk/baseline.env
. "$PATCH_DIR/baseline.env"

fail() { echo "ERROR: $*" >&2; exit 1; }
sha() { shasum -a 256 "$1" | cut -d' ' -f1; }

check_stamp() {  # SDK_DIR
  local sdk="$1" stamp="$1/$STAMP_NAME" problems=0
  [ -f "$stamp" ] || { echo "  $STAMP_NAME is missing" >&2; return 1; }
  [ "$(sed -n 's/^sdk_commit=//p' "$stamp")" = "$SDK_COMMIT" ] ||
    { echo "  plugin was not built from ReXGlue $SDK_TAG ($SDK_COMMIT)" >&2; problems=1; }
  [ "$(sed -n 's/^patch_sha256=//p' "$stamp")" = "$(sha "$PATCH")" ] ||
    { echo "  plugin was built with a different patch than patches/rexglue-sdk (rebuild it)" >&2; problems=1; }
  [ -f "$sdk/$LIB" ] || { echo "  missing: $LIB" >&2; return 1; }
  local actual
  actual="$(sha "$sdk/$LIB")"
  grep -qx "$actual  $LIB" "$stamp" || { echo "  $LIB does not match the patched build" >&2; problems=1; }
  [ "$actual" != "$OFFICIAL_MAC_ARM64_GPU_SHA256" ] || { echo "  $LIB is the stock ReXGlue release file" >&2; problems=1; }
  return "$problems"
}

MODE=build
case "${1:-}" in
  --check | --restore) MODE="${1#--}"; shift ;;
esac
SDK="${1:-$ROOT/tools/rexglue/mac-arm64}"
[ -d "$SDK/lib/cmake/rexglue" ] || fail "$SDK is not an extracted ReXGlue SDK (run tools/codegen.sh first)"
[ "$(basename "$SDK")" = mac-arm64 ] || fail "only the mac-arm64 SDK is supported"

if [ "$MODE" = check ]; then
  check_stamp "$SDK" || fail "$SDK does not hold the patched ReXGlue GPU plugin"
  exit 0
fi

if [ "$MODE" = restore ]; then
  [ -f "$SDK/$STOCK" ] || fail "no saved stock plugin in $SDK (delete the SDK folder and run tools/codegen.sh)"
  cp "$SDK/$STOCK" "$SDK/$LIB"
  rm -f "$SDK/$STAMP_NAME"
  echo "Stock ReXGlue GPU plugin restored: $SDK/$LIB"
  exit 0
fi

[ "$(uname -s)" = Darwin ] || fail "builds on macOS only"
SRC="${2:-$ROOT/out/rexglue-sdk-src}"
for tool in git clang++ cmake ninja shasum nm; do
  command -v "$tool" >/dev/null || fail "$tool not found"
done

if [ ! -d "$SRC/.git" ]; then
  echo "== Fetching ReXGlue $SDK_TAG from $SDK_REPO (network) into $SRC"
  mkdir -p "$(dirname "$SRC")"
  git clone --branch "$SDK_TAG" --depth 1 "$SDK_REPO" "$SRC"
fi
COMMIT="$(git -C "$SRC" rev-parse HEAD)"
[ "$COMMIT" = "$SDK_COMMIT" ] || fail "$SRC is at $COMMIT; expected $SDK_TAG $SDK_COMMIT (delete the folder to fetch it again)"
# Only glslang's headers are needed; its libraries come with the SDK.
[ -f "$SRC/thirdparty/glslang/glslang/Public/ShaderLang.h" ] ||
  git -C "$SRC" submodule update --init --depth 1 thirdparty/glslang

echo "== Patch"
git -C "$SRC" checkout -- src
git -C "$SRC" apply --check "$PATCH" || fail "$(basename "$PATCH") no longer applies to ReXGlue $SDK_TAG"
git -C "$SRC" apply "$PATCH"

echo "== Build librexgpu-xenos.dylib (Release)"
BUILD="$SRC/out/build/rexgpu-xenos-mac-arm64"
# -ffile-prefix-map keeps the local source and SDK folders out of the plugin's log and assert strings.
FLAGS="-ffile-prefix-map=$SRC=. -ffile-prefix-map=$SDK=."
cmake -S "$ROOT/tools/rexgpu-xenos" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_OSX_ARCHITECTURES=arm64 \
  "-DCMAKE_C_FLAGS=$FLAGS" "-DCMAKE_CXX_FLAGS=$FLAGS" "-DREXGLUE_SRC=$SRC" "-DREXGLUE_SDK=$SDK" >/dev/null
cmake --build "$BUILD" --parallel
BUILT="$BUILD/librexgpu-xenos.dylib"
[ -f "$BUILT" ] || fail "the build output is missing $BUILT"
for path in "$SRC" "$SDK" "$ROOT" "$HOME"; do
  ! grep -qaF "$path" "$BUILT" || fail "librexgpu-xenos.dylib embeds a local folder ($path)"
done

# Keep the stock plugin once, so --restore and the export check have it.
if [ ! -f "$SDK/$STOCK" ]; then
  [ "$(sha "$SDK/$LIB")" = "$OFFICIAL_MAC_ARM64_GPU_SHA256" ] ||
    fail "$SDK/$LIB is not the stock plugin and no stock copy was saved (delete the SDK folder and run tools/codegen.sh)"
  cp "$SDK/$LIB" "$SDK/$STOCK"
fi
# The runtime loads the plugin by its exports; they must match the release build's.
diff <(nm -gU "$SDK/$STOCK" | awk '{print $3}' | sort) <(nm -gU "$BUILT" | awk '{print $3}' | sort) >/dev/null ||
  fail "the built plugin exports different symbols than the stock release"

cp "$BUILT" "$SDK/$LIB"
{
  echo "sdk_commit=$SDK_COMMIT"
  echo "patch_sha256=$(sha "$PATCH")"
  echo "$(sha "$SDK/$LIB")  $LIB"
} > "$SDK/$STAMP_NAME"
check_stamp "$SDK" || fail "the installed plugin failed its own check"
echo "Patched ReXGlue GPU plugin installed: $SDK/$LIB"
cat "$SDK/$STAMP_NAME"
