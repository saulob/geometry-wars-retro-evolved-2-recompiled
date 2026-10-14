#!/usr/bin/env bash
# Downloads the pinned ReXGlue SDK, checks game/extracted/default.xex and generates the recompiled C++ sources
# into generated/default. The Linux counterpart of tools/codegen.ps1.
#
#   tools/codegen.sh    SDK for this machine: linux-amd64 (REXGLUE_PLATFORM overrides it)
#
# Needs curl or wget, and unzip or python3.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

SDK_VERSION=0.10.0
XEX_SHA256=6d5d9980a1d17dc74b9ccf540cec16c2e144e7cee3a74938e78c237b8aceeb88
MANIFEST=geometry_wars_retro_evolved_2_manifest.toml

fail() { echo "ERROR: $*" >&2; exit 1; }

sha256() { sha256sum "$1" | cut -d' ' -f1; }

if [ -z "${REXGLUE_PLATFORM:-}" ]; then
  case "$(uname -s)-$(uname -m)" in
    Linux-x86_64) REXGLUE_PLATFORM=linux-amd64 ;;
    *) fail "no ReXGlue SDK for $(uname -s) $(uname -m); set REXGLUE_PLATFORM" ;;
  esac
fi
case "$REXGLUE_PLATFORM" in
  linux-amd64) SDK_SHA256=0388edb3f8fb1444f1b0ad5d440cb02d9690ac20a76b04fd6b7dcc19f59328a6 ;;
  *) fail "unknown REXGLUE_PLATFORM $REXGLUE_PLATFORM" ;;
esac

SDK_DIR="$ROOT/tools/rexglue"
SDK="$SDK_DIR/$REXGLUE_PLATFORM"
REXGLUE="$SDK/bin/rexglue"
XEX="$ROOT/game/extracted/default.xex"
LOG="$ROOT/generated/codegen.log"

if [ ! -x "$REXGLUE" ]; then
  mkdir -p "$SDK_DIR"
  ZIP="$SDK_DIR/rexglue-sdk-$SDK_VERSION-$REXGLUE_PLATFORM.zip"
  if [ ! -f "$ZIP" ] || [ "$(sha256 "$ZIP")" != "$SDK_SHA256" ]; then
    rm -f "$ZIP"
    URL="https://github.com/rexglue/rexglue-sdk/releases/download/v$SDK_VERSION/rexglue-sdk-$SDK_VERSION-$REXGLUE_PLATFORM.zip"
    echo "Downloading ReXGlue SDK $SDK_VERSION ($REXGLUE_PLATFORM)..."
    if command -v curl >/dev/null; then curl -fsSL -o "$ZIP" "$URL"; else wget -q -O "$ZIP" "$URL"; fi
  fi
  [ "$(sha256 "$ZIP")" = "$SDK_SHA256" ] || fail "$ZIP is not the official ReXGlue $SDK_VERSION release"
  rm -rf "$SDK"
  if command -v unzip >/dev/null; then
    unzip -q -o "$ZIP" -d "$SDK_DIR"
  else
    python3 -I -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' "$ZIP" "$SDK_DIR"
    chmod +x "$REXGLUE"
  fi
fi
INSTALLED="$("$REXGLUE" --version | tr -d '[:space:]')"
[ "$INSTALLED" = "$SDK_VERSION" ] ||
  fail "tools/rexglue/$REXGLUE_PLATFORM has ReXGlue $INSTALLED; expected $SDK_VERSION (delete it to re-download)"

[ -f "$XEX" ] || fail "missing $XEX (extract the package into game/extracted first: tools/extract_stfs.sh)"
HASH="$(sha256 "$XEX")"
[ "$HASH" = "$XEX_SHA256" ] || fail "unsupported default.xex (SHA-256 $HASH); expected $XEX_SHA256"
echo "default.xex is the supported version."

mkdir -p "$(dirname "$LOG")"
cd "$ROOT"
START=$SECONDS
set +e
"$REXGLUE" codegen "$MANIFEST" 2>&1 | tee "$LOG"
EXIT_CODE=${PIPESTATUS[0]}
set -e
echo "codegen exit code $EXIT_CODE after $((SECONDS - START)) s (log: $LOG)"
[ "$EXIT_CODE" = 0 ] || fail "codegen failed ($EXIT_CODE)"
