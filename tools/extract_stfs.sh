#!/usr/bin/env bash
# Extracts an Xbox 360 STFS (LIVE, PIRS or CON) package into an empty folder on Linux.
# The package must carry the Geometry Wars: Retro Evolved 2 title ID.
#
#   tools/extract_stfs.sh <package> game/extracted
#
# Compiles tools/extract_stfs/main.cpp with the launcher's src/stfs.cpp; needs clang++ with C++23.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ "$#" = 2 ] || { echo "usage: $0 <package> <out dir>" >&2; exit 2; }

BIN="$ROOT/out/tools/extract_stfs"
mkdir -p "$(dirname "$BIN")"
# shellcheck disable=SC2086 # CXXFLAGS holds several flags
"${CXX:-clang++}" ${CXXFLAGS:-} -std=c++23 -O2 -I"$ROOT/src" -o "$BIN" "$ROOT/tools/extract_stfs/main.cpp" "$ROOT/src/stfs.cpp"
"$BIN" "$1" "$2"
