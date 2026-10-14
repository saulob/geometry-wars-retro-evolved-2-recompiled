#!/usr/bin/env bash
# Prepares an extracted linux-amd64 ReXGlue SDK for this branch: builds the ReXGlue targets that the Linux patches in
# patches/rexglue-sdk/patches.txt change (librexruntime.so, librexgpu-xenos.so) from the pinned v0.10.0 source, and
# puts the stock release files back for the targets no patch changes. tools/build.sh runs it when the SDK does not
# match, and the CMake build checks the same thing with --check. See docs/rexglue-runtime-patch.md.
#
#   tools/build-patched-rexruntime.sh SDK_DIR [WORK_DIR] [--config CONFIG]...   prepare (network needed the first time)
#   tools/build-patched-rexruntime.sh --check SDK_DIR [--config CONFIG] [--staged DIR]   verify only
#   tools/build-patched-rexruntime.sh --stress SDK_DIR BUILD_DIR   run the allocator stress test (see below)
#   tools/build-patched-rexruntime.sh --loader-test SDK_DIR BUILD_DIR   run the RenderDoc detection test (see below)
#
# CONFIG is Debug, RelWithDebInfo or Release (default Release). WORK_DIR holds the fetched source and its build
# (default: out/rexglue-sdk-src in this repository). Needs git, clang-20, clang++-20, cmake 3.25+, ninja, binutils and
# the packages listed in docs/rexglue-runtime-patch.md. --stress and --loader-test run gw2_physical_heap_stress and
# gw2_renderdoc_detect_tests from an already configured Release project build in BUILD_DIR, beside the installed
# runtime; --loader-test also runs its test against the stock runtime, which must fail.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PATCH_DIR="$ROOT/patches/rexglue-sdk"
PLATFORM=linux
ALL_TARGETS=(rexruntime rexgpu-xenos)
STAMP_NAME=rexruntime-patch.txt
CC=clang-20
CXX=clang++-20
FLAGS_TEMPLATE='-march=x86-64-v2 -ffile-prefix-map=<source>=.'
# The runtime loads the plugin through these.
PLUGIN_ENTRY_POINTS=(rex_gpu_abi_version rex_gpu_create)

# shellcheck source=../patches/rexglue-sdk/baseline.env
. "$PATCH_DIR/baseline.env"

fail() { echo "ERROR: $*" >&2; exit 1; }
sha() { sha256sum "$1" | cut -d' ' -f1; }

# Reads patches.txt into SELECTED (this platform's patches, in order, as "name targets") and PATCHED_TARGETS.
read_manifest() {
  local LC_ALL=C line name platforms targets extra item previous="" listed="" files duplicates
  SELECTED=()
  PATCHED_TARGETS=()
  files="$(find "$PATCH_DIR" -maxdepth 1 -name '*.patch' -printf '%f\n' | sort)"
  duplicates="$(cut -c1-4 <<<"$files" | uniq -d)"
  [ -z "$duplicates" ] || fail "patches/rexglue-sdk has more than one patch numbered ${duplicates//$'\n'/ }"
  while IFS= read -r line || [ -n "$line" ]; do
    [[ "$line" =~ ^[[:space:]]*(#|$) ]] && continue
    read -r name platforms targets extra <<<"$line"
    [[ -n "$targets" && -z "$extra" && "$name" =~ ^[0-9]{4}-[^[:space:]]+\.patch$ ]] || fail "patches.txt: cannot read '$line'"
    for item in ${platforms//,/ }; do
      case "$item" in windows | linux) ;; *) fail "patches.txt: unknown platform '$item' for $name" ;; esac
    done
    for item in ${targets//,/ }; do
      case "$item" in rexruntime | rexgpu-xenos) ;; *) fail "patches.txt: unknown target '$item' for $name" ;; esac
    done
    [[ "$name" > "$previous" ]] || fail "patches.txt must list the patches in numeric order ($name)"
    previous="$name"
    [ -f "$PATCH_DIR/$name" ] || fail "patches.txt lists $name, which is missing"
    listed+=" $name "
    [[ ",$platforms," == *",$PLATFORM,"* ]] && SELECTED+=("$name ${targets//,/ }")
  done < "$PATCH_DIR/patches.txt"
  for name in $files; do
    [[ "$listed" == *" $name "* ]] || fail "patches/rexglue-sdk/patches.txt does not list $name"
  done
  for item in "${ALL_TARGETS[@]}"; do
    [[ " ${SELECTED[*]} " == *" $item "* ]] && PATCHED_TARGETS+=("$item")
  done
  return 0
}

is_patched() { [[ " ${PATCHED_TARGETS[*]} " == *" $1 "* ]]; }

suffix() {  # CONFIG
  case "$1" in Debug) echo d ;; RelWithDebInfo) echo rd ;; Release) echo ;; *) fail "unknown configuration '$1'" ;; esac
}

target_lib() { echo "lib/lib$1$(suffix "$2").so"; }  # TARGET CONFIG

official_sha() {  # TARGET CONFIG
  local kind=RUNTIME variant
  [ "$1" = rexgpu-xenos ] && kind=GPU
  case "$2" in Debug) variant=_D ;; RelWithDebInfo) variant=_RD ;; *) variant= ;; esac
  local name="OFFICIAL_LINUX_$kind${variant}_SHA256"
  echo "${!name}"
}

# Everything the patched files are built from besides the source commit's own files. A stamp with other inputs is stale.
inputs() {
  local entry name
  echo "sdk_version=$SDK_VERSION"
  echo "sdk_tag=$SDK_TAG"
  echo "sdk_commit=$SDK_COMMIT"
  echo "platform=$PLATFORM"
  for entry in "${SELECTED[@]}"; do
    name="${entry%% *}"
    local targets="${entry#* }"
    echo "patch=$name $(sha "$PATCH_DIR/$name") ${targets// /,}"
  done
  echo "targets=${PATCHED_TARGETS[*]}"
  echo "compiler=$("$CXX" --version | sed -n 1p)"
  echo "flags=$FLAGS_TEMPLATE"
}

# Every RUNPATH entry must be relative to the library itself.
check_runpath() {  # FILE
  local entry
  for entry in $(readelf -d "$1" | sed -n 's/.*(R\(UN\)\{0,1\}PATH).*\[\(.*\)\]/\2/p' | tr ':' ' '); do
    [[ "$entry" == '$ORIGIN'* ]] || { echo "  $(basename "$1") has the RUNPATH entry $entry" >&2; return 1; }
  done
}

stamp_inputs() { sed '/^[0-9a-f]\{64\}  /d' "$1"; }

check_stamp() {  # SDK_DIR CONFIG [STAGED_DIR]
  local sdk="$1" config="$2" staged="${3:-}" stamp="$1/$STAMP_NAME" problems=0 target lib file actual recorded
  command -v "$CXX" >/dev/null || { echo "  $CXX not found" >&2; return 1; }
  if [ "${#PATCHED_TARGETS[@]}" -gt 0 ]; then
    if [ ! -f "$stamp" ]; then
      echo "  $STAMP_NAME is missing" >&2
      problems=1
    elif [ "$(stamp_inputs "$stamp")" != "$(inputs)" ]; then
      echo "  built from other inputs than this branch's:" >&2
      diff <(stamp_inputs "$stamp") <(inputs) | sed 's/^/    /' >&2 || true
      problems=1
    fi
  fi
  for target in "${ALL_TARGETS[@]}"; do
    lib="$(target_lib "$target" "$config")"
    file="$sdk/$lib"
    [ -f "$file" ] || { echo "  missing: $lib" >&2; problems=1; continue; }
    actual="$(sha "$file")"
    if ! is_patched "$target"; then
      [ "$actual" = "$(official_sha "$target" "$config")" ] ||
        { echo "  $lib is not the stock ReXGlue release file, and no Linux patch changes $target" >&2; problems=1; }
    elif [ "$actual" = "$(official_sha "$target" "$config")" ]; then
      echo "  $lib is the stock ReXGlue release file" >&2
      problems=1
    else
      recorded=""
      if [ -f "$stamp" ]; then recorded="$(sed -n "s|^\([0-9a-f]\{64\}\)  $lib\$|\1|p" "$stamp")"; fi
      [ "$actual" = "$recorded" ] || { echo "  $lib does not match the patched build" >&2; problems=1; }
      check_runpath "$file" || problems=1
    fi
    if [ -n "$staged" ]; then
      if [ ! -f "$staged/$(basename "$lib")" ]; then
        echo "  $(basename "$lib") is not next to the program in $staged" >&2
        problems=1
      elif [ "$(sha "$staged/$(basename "$lib")")" != "$actual" ]; then
        echo "  $(basename "$lib") next to the program is not the one in the SDK" >&2
        problems=1
      fi
    fi
  done
  return "$problems"
}

stock_file() {  # SDK_DIR TARGET CONFIG OUT_FILE: the stock file, from the SDK or from its download kept beside SDK_DIR
  local lib zip official
  lib="$(target_lib "$2" "$3")"
  official="$(official_sha "$2" "$3")"
  if [ -f "$1/$lib" ] && [ "$(sha "$1/$lib")" = "$official" ]; then
    cp "$1/$lib" "$4"
  else
    zip="$(dirname "$1")/rexglue-sdk-$SDK_VERSION-linux-amd64.zip"
    [ -f "$zip" ] || return 1
    if command -v unzip >/dev/null; then
      unzip -p "$zip" "linux-amd64/$lib" > "$4"
    else
      python3 -I -c 'import sys, zipfile; sys.stdout.buffer.write(zipfile.ZipFile(sys.argv[1]).read(sys.argv[2]))' \
        "$zip" "linux-amd64/$lib" > "$4"
    fi
  fi
  [ "$(sha "$4")" = "$official" ]
}

# Weak symbols are left out: which inline and template copies are exported depends on the C++ library headers.
strong_exports() {  # FILE
  nm -D --defined-only "$1" | awk '$2 !~ /^[VvWw]$/ {print $3}' | LC_ALL=C sort
}

MODE=build
SDK=""
SRC=""
STAGED=""
CONFIGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --check | --stress | --loader-test) MODE="${1#--}" ;;
    --config) CONFIGS+=("${2:?--config needs a configuration}"); shift ;;
    --staged) STAGED="${2:?--staged needs a folder}"; shift ;;
    -*) fail "unknown option $1" ;;
    *) if [ -z "$SDK" ]; then SDK="$1"; elif [ -z "$SRC" ]; then SRC="$1"; else fail "unexpected argument $1"; fi ;;
  esac
  shift
done
[ "${#CONFIGS[@]}" -gt 0 ] || CONFIGS=(Release)
for config in "${CONFIGS[@]}"; do suffix "$config" >/dev/null; done
[ -n "$SDK" ] && [ -d "$SDK/lib/cmake/rexglue" ] || fail "usage: see the top of $0 (SDK_DIR is the extracted linux-amd64 SDK)"
read_manifest

if [ "$MODE" = check ]; then
  problems=0
  for config in "${CONFIGS[@]}"; do
    check_stamp "$SDK" "$config" "$STAGED" || problems=1
  done
  [ "$problems" = 0 ] && exit 0
  echo "The ReXGlue SDK or the build folder does not match this branch (${CONFIGS[*]}). Prepare it with:" >&2
  echo "  tools/build.sh, or tools/build-patched-rexruntime.sh $SDK --config ${CONFIGS[0]}" >&2
  exit 1
fi

if [ "$MODE" = stress ]; then
  [ -n "$SRC" ] || fail "usage: $0 --stress SDK_DIR BUILD_DIR"
  check_stamp "$SDK" Release || fail "$SDK does not hold the patched ReXGlue libraries"
  cmake --build "$SRC" --target gw2_physical_heap_stress
  # Run beside the installed runtime, not the copy the build left in BUILD_DIR, which may be stale.
  stage="$SRC/physical_heap_stress"
  mkdir -p "$stage"
  cp "$SRC/gw2_physical_heap_stress" "$SDK/lib/librexruntime.so" "$stage/"
  (cd "$stage" && ./gw2_physical_heap_stress | grep -E '^(view|PASS|FAIL)')
  exit 0
fi

stage_loader_test() {  # STAGE_DIR RUNTIME: the test, its fake RenderDoc and the runtime under test
  rm -rf "$1"
  mkdir -p "$1/renderdoc"
  cp "$SRC/gw2_renderdoc_detect_tests" "$1/"
  cp "$SRC/renderdoc/librenderdoc.so" "$1/renderdoc/"
  cp "$2" "$1/librexruntime.so"
}

if [ "$MODE" = loader-test ]; then
  [ -n "$SRC" ] || fail "usage: $0 --loader-test SDK_DIR BUILD_DIR"
  check_stamp "$SDK" Release || fail "$SDK does not hold the patched ReXGlue libraries"
  cmake --build "$SRC" --target gw2_renderdoc_detect_tests
  stage="$SRC/renderdoc_detect"
  stage_loader_test "$stage" "$SDK/lib/librexruntime.so"
  echo "== Patched runtime (must pass)"
  (cd "$stage" && ./gw2_renderdoc_detect_tests)
  # The stock runtime must fail by loading the discoverable fake itself, which shows the test detects the bug.
  stock="$SRC/renderdoc_detect_stock"
  stage_loader_test "$stock" "$SDK/lib/librexruntime.so"
  stock_file "$SDK" rexruntime Release "$stock/librexruntime.so" ||
    fail "no stock librexruntime.so for the control run: keep rexglue-sdk-$SDK_VERSION-linux-amd64.zip beside $SDK"
  echo "== Stock runtime (must fail)"
  if output="$(cd "$stock" && ./gw2_renderdoc_detect_tests 2>&1)"; then
    echo "$output"
    fail "the test passed against the stock runtime, so it does not detect the bug"
  fi
  echo "$output"
  grep -qxE 'FAIL: (absent|not loaded): CreateIfConnected returns nullptr' <<< "$output" ||
    fail "the stock runtime failed the test, but not by loading RenderDoc itself"
  exit 0
fi

SRC="${SRC:-$ROOT/out/rexglue-sdk-src}"
for tool in git "$CC" "$CXX" cmake ninja sha256sum readelf nm; do
  command -v "$tool" >/dev/null || fail "$tool not found"
done
INSTALLED=()

if [ "${#PATCHED_TARGETS[@]}" -gt 0 ]; then
  if [ ! -d "$SRC/.git" ]; then
    echo "== Fetching ReXGlue $SDK_TAG from $SDK_REPO (network) into $SRC"
    mkdir -p "$(dirname "$SRC")"
    git clone --branch "$SDK_TAG" --depth 1 --recurse-submodules --shallow-submodules --jobs 8 "$SDK_REPO" "$SRC"
  fi
  COMMIT="$(git -C "$SRC" rev-parse HEAD)"
  [ "$COMMIT" = "$SDK_COMMIT" ] || fail "$SRC is at $COMMIT; expected $SDK_TAG $SDK_COMMIT (delete the folder to fetch it again)"

  echo "== Patch"
  # Another branch may have left other patches in the tree, in any tracked file.
  git -C "$SRC" checkout -- .
  git -C "$SRC" clean -fdq -- src include tests cmake
  for entry in "${SELECTED[@]}"; do
    name="${entry%% *}"
    git -C "$SRC" apply --check "$PATCH_DIR/$name" || fail "$name no longer applies to ReXGlue $SDK_TAG"
    git -C "$SRC" apply "$PATCH_DIR/$name"
    echo "  $name"
  done

  echo "== Build ${PATCHED_TARGETS[*]} (${CONFIGS[*]})"
  FLAGS="${FLAGS_TEMPLATE/<source>/$SRC}"
  (
    cd "$SRC"
    # The libraries are taken from the build tree, which lies outside the binary folder, so CMake would give them an
    # absolute build RUNPATH; the install RUNPATH ($ORIGIN) is what the release files carry.
    cmake --preset linux-amd64 "-DCMAKE_C_COMPILER=$CC" "-DCMAKE_CXX_COMPILER=$CXX" \
      "-DCMAKE_C_FLAGS=$FLAGS" "-DCMAKE_CXX_FLAGS=$FLAGS" -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON >/dev/null
    for config in "${CONFIGS[@]}"; do
      cmake --build out/build/linux-amd64 --config "$config" --target "${PATCHED_TARGETS[@]}" --parallel
    done
  )
  for config in "${CONFIGS[@]}"; do
    OUT="$SRC/out/linux-amd64/$config"
    for target in "${PATCHED_TARGETS[@]}"; do
      lib="$(target_lib "$target" "$config")"
      built="$OUT/$(basename "$lib")"
      [ -f "$built" ] || fail "ReXGlue build output is missing $built"
      [ "$(sha "$built")" != "$(official_sha "$target" "$config")" ] ||
        fail "$(basename "$lib") came out identical to the stock release; the patches were not built in"
      # Only Release is released; the other configurations keep their debug information's paths.
      if [ "$config" = Release ]; then
        for path in "$SRC" "$SDK" "$ROOT"; do
          ! grep -qaF "$path" "$built" || fail "$(basename "$lib") embeds a local folder ($path)"
        done
      fi
      check_runpath "$built" || fail "$(basename "$lib") would look for libraries outside its own folder"
    done
    if is_patched rexgpu-xenos; then
      plugin="$OUT/$(basename "$(target_lib rexgpu-xenos "$config")")"
      readelf -d "$plugin" > "$plugin.dynamic.txt"
      grep -qE '\(NEEDED\).*\[librexruntime[a-z]*\.so\]' "$plugin.dynamic.txt" ||
        fail "$(basename "$plugin") does not link the ReXGlue runtime"
      strong_exports "$plugin" > "$plugin.exports.txt"
      for symbol in "${PLUGIN_ENTRY_POINTS[@]}"; do
        grep -qx "$symbol" "$plugin.exports.txt" || fail "$(basename "$plugin") does not export $symbol"
      done
      stock_file "$SDK" rexgpu-xenos "$config" "$plugin.stock" ||
        fail "no stock $(basename "$plugin") to compare with: keep rexglue-sdk-$SDK_VERSION-linux-amd64.zip beside $SDK"
      strong_exports "$plugin.stock" > "$plugin.stock-exports.txt"
      diff "$plugin.stock-exports.txt" "$plugin.exports.txt" >&2 ||
        fail "$(basename "$plugin") exports different symbols than the stock release (above: < stock, > built)"
    fi
  done
  for config in "${CONFIGS[@]}"; do
    for target in "${PATCHED_TARGETS[@]}"; do
      lib="$(target_lib "$target" "$config")"
      cp "$SRC/out/linux-amd64/$config/$(basename "$lib")" "$SDK/$lib"
      INSTALLED+=("$lib")
    done
  done
fi

for config in "${CONFIGS[@]}"; do
  for target in "${ALL_TARGETS[@]}"; do
    is_patched "$target" && continue
    lib="$(target_lib "$target" "$config")"
    [ -f "$SDK/$lib" ] && [ "$(sha "$SDK/$lib")" = "$(official_sha "$target" "$config")" ] && continue
    stock_file "$SDK" "$target" "$config" "$SDK/$lib.tmp" || fail "cannot restore the stock $lib (keep the SDK download beside $SDK)"
    mv "$SDK/$lib.tmp" "$SDK/$lib"
    echo "  restored the stock $lib"
  done
done

# Files of other configurations stay recorded only if they were built from the same inputs.
STAMP="$SDK/$STAMP_NAME"
KEEP=""
[ -f "$STAMP" ] && [ "$(stamp_inputs "$STAMP")" = "$(inputs)" ] && KEEP="$(grep -E '^[0-9a-f]{64}  ' "$STAMP" || true)"
{
  inputs
  {
    while IFS= read -r line; do
      if [ -n "$line" ] && [[ " ${INSTALLED[*]} " != *" ${line#*  } "* ]]; then echo "$line"; fi
    done <<<"$KEEP"
    for lib in "${INSTALLED[@]}"; do echo "$(sha "$SDK/$lib")  $lib"; done
  } | LC_ALL=C sort -k2,2
} > "$STAMP.tmp"
mv "$STAMP.tmp" "$STAMP"
for config in "${CONFIGS[@]}"; do
  check_stamp "$SDK" "$config" || fail "the installed libraries failed their own check ($config)"
done
echo "ReXGlue prepared for ${CONFIGS[*]} in $SDK"
cat "$STAMP"
