#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${WORK:-/mnt/HaikuWork}
CROSS=${CROSS:-$WORK/build/arm64/cross-tools-arm64/bin/aarch64-unknown-haiku-}
SYSROOT=${SYSROOT:-$WORK/build/summit-arm64/sysroot}
TOOLS=${TOOLS:-$WORK/build/arm64/objects/linux/x86_64/release/tools}
HAIKU_TREE=${HAIKU_TREE:-$WORK/rpi4/haiku}
HAIKU_BUILD=${HAIKU_BUILD:-$WORK/rpi4/build/objects/haiku/arm64/release}
mkdir -p "$ROOT/build-arm64/.hostbin"
SPECS=$ROOT/build-arm64/.hostbin/shared-unwinder.specs
printf '*libgcc:\n-lgcc_s -lgcc\n\n' > "$SPECS"
make -C "$ROOT" -j"${JOBS:-8}" BUILD=build-arm64 \
    CXX="${CROSS}g++ --sysroot=$SYSROOT -specs=$SPECS" \
    CPPFLAGS="-Isrc -I$HAIKU_TREE/headers/os/device -I$HAIKU_TREE/headers/os/drivers -I$SYSROOT/boot/system/develop/headers/private/netservices" \
    LDFLAGS="-L$HAIKU_BUILD/kits/device" \
    RC="$TOOLS/rc/rc" XRES="$TOOLS/xres" MIMESET=true "${@:-all}"
