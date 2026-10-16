#!/usr/bin/env bash
# Build VineOS AOSP ROM images (ranchu) for 7.1.2 / 9 / 11 / 12 and package .vrom files.
# Usage: scripts/build_rom.sh [7|9|11|12|all]
# Host: Ubuntu 20.04/22.04 x86_64, 16GB+ RAM, 300GB+ disk. See docs/BUILDING.md.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/rom-out}"
AOSP_BASE="${AOSP_BASE:-$HOME/aosp-vine}"

declare -A BRANCH=( [7]="android-7.1.2_r39" [9]="android-9.0.0_r61" [11]="android-11.0.0_r48" [12]="android-12.0.0_r34" )
declare -A LUNCH=( [7]="aosp_arm64-userdebug" [9]="aosp_arm64-userdebug" [11]="aosp_arm64-userdebug" [12]="aosp_arm64-userdebug" )
declare -A VNROM=( [7]="vine-rom-7" [9]="vine-rom-9" [11]="vine-rom-11" [12]="vine-rom-12" )

build_one() {
  local ver="$1"
  local src="$AOSP_BASE/$ver"
  mkdir -p "$src" "$OUT_DIR"
  echo "=== VineOS ROM $ver (${BRANCH[$ver]}) ==="
  if [ ! -d "$src/.repo" ]; then
    ( cd "$src" && repo init -u https://android.googlesource.com/platform/manifest -b "${BRANCH[$ver]}" --depth=1 )
  fi
  ( cd "$src" && repo sync -c -j"$(nproc)" --no-tags --no-clone-bundle )
  if [ -d "$REPO_ROOT/rom-patches/android-$ver" ]; then
    ( cd "$src" && git apply "$REPO_ROOT"/rom-patches/android-"$ver"/*.patch || true )
  fi
  ( cd "$src" && source build/envsetup.sh && lunch "${LUNCH[$ver]}" && make -j"$(nproc)" )
  local prod="$src/out/target/product/generic_arm64"
  local stage="$OUT_DIR/${VNROM[$ver]}"
  mkdir -p "$stage"
  cp "$prod/system.img" "$prod/ramdisk.img" "$stage/" 2>/dev/null || true
  cp "$prod/vendor.img" "$stage/" 2>/dev/null || true
  cp "$prod/kernel" "$stage/" 2>/dev/null || true
  cat > "$stage/manifest.json" <<EOF
{
  "displayName": "VineOS ROM $ver",
  "androidVersion": "$ver",
  "apiLevel": $([ "$ver" = "7" ] && echo 25 || ([ "$ver" = "9" ] && echo 28 || ([ "$ver" = "11" ] && echo 30 || echo 31))),
  "supportedAbis": ["arm64-v8a", "armeabi-v7a"],
  "has32BitSupport": true,
  "releaseDate": "$(date +%Y-%m-%d)"
}
EOF
  ( cd "$stage" && zip -0 "$OUT_DIR/${VNROM[$ver]}.vrom" system.img vendor.img ramdisk.img kernel 2>/dev/null || zip -0 "$OUT_DIR/${VNROM[$ver]}.vrom" system.img ramdisk.img kernel )
  ( cd "$stage" && zip "$OUT_DIR/${VNROM[$ver]}.vrom" manifest.json )
  echo "Wrote $OUT_DIR/${VNROM[$ver]}.vrom"
}

TARGET="${1:-all}"
if [ "$TARGET" = "all" ]; then
  for v in 7 9 11 12; do build_one "$v"; done
else
  build_one "$TARGET"
fi
