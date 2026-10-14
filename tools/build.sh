#!/usr/bin/env bash
# The normal Linux and macOS build. Makes sure tools/rexglue/linux-amd64 (mac-arm64 on macOS) holds the ReXGlue runtime
# and GPU plugin this branch's patches need for the configuration, building them with tools/build-patched-rexruntime.sh
# when it does not (git and network access the first time), then configures and builds the matching preset into
# out/build/<preset>.
#
#   tools/build.sh                  linux-amd64-release (mac-arm64-release on macOS)
#   tools/build.sh relwithdebinfo   linux-amd64-relwithdebinfo (or debug)
#
# Run tools/codegen.sh first. See docs/linux.md and docs/macos.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PATCHED="$ROOT/tools/build-patched-rexruntime.sh"

fail() { echo "ERROR: $*" >&2; exit 1; }

case "${1:-release}" in
  release) CONFIG=Release ;;
  relwithdebinfo) CONFIG=RelWithDebInfo ;;
  debug) CONFIG=Debug ;;
  *) fail "usage: $0 [release|relwithdebinfo|debug]" ;;
esac
case "$(uname -s)" in
  Linux) PLATFORM=linux-amd64 ;;
  Darwin) PLATFORM=mac-arm64 ;;
  *) fail "builds on Linux and macOS only" ;;
esac
SDK="$ROOT/tools/rexglue/$PLATFORM"
PRESET="$PLATFORM-${1:-release}"
[ -d "$SDK/lib/cmake/rexglue" ] || fail "missing $SDK (run tools/codegen.sh first)"

if ! bash "$PATCHED" --check "$SDK" --config "$CONFIG"; then
  echo "== Preparing the ReXGlue runtime and GPU plugin for this branch ($CONFIG)"
  bash "$PATCHED" "$SDK" --config "$CONFIG"
fi

cd "$ROOT"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
echo "Built out/build/$PRESET/geometry_wars_retro_evolved_2"
