#!/usr/bin/env bash
# Package a built executable. On Linux use the existing Haiku host tools.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARCH=${1:?Usage: package.sh arm64|x86_64 BUILD_DIRECTORY}
case "$ARCH" in arm64|x86_64) ;; *) echo 'Unsupported architecture' >&2; exit 1;; esac
BUILD=${2:?Supply the build directory}
[[ $BUILD = /* ]] || BUILD="$ROOT/$BUILD"
if [[ $(uname -s) = Haiku ]]; then
    PACKAGE=${PACKAGE:-package}; RESATTR=${RESATTR:-resattr}
else
    TOOLS=${TOOLS:-/mnt/HaikuWork/build/arm64/objects/linux/x86_64/release/tools}
    PACKAGE=${PACKAGE:-$TOOLS/package/package}; RESATTR=${RESATTR:-$TOOLS/resattr/resattr}
fi
STAGE=$(mktemp -d "$BUILD/package-stage.XXXXXX")
mkdir -p "$STAGE/apps/Weather" "$STAGE/data/deskbar/menu/Applications" \
    "$STAGE/data/licenses" "$STAGE/documentation/packages/weather" "$ROOT/artifacts"
cp "$BUILD/Weather" "$STAGE/apps/Weather/Weather"
chmod 755 "$STAGE/apps/Weather/Weather"
cp -R "$ROOT/resources" "$STAGE/apps/Weather/"
"$RESATTR" -O -o "$STAGE/apps/Weather/Weather" "$BUILD/Weather.rsrc"
ln -s ../../../../apps/Weather/Weather "$STAGE/data/deskbar/menu/Applications/Weather"
cp "$ROOT/LICENSE" "$STAGE/data/licenses/MIT"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$STAGE/documentation/packages/weather/"
cp -R "$ROOT/docs" "$STAGE/documentation/packages/weather/"
sed "s/@ARCH@/$ARCH/g" "$ROOT/resources/Weather.PackageInfo.in" > "$STAGE/.PackageInfo"
FILE="$ROOT/artifacts/weather-1.0.0~beta1-2-$ARCH.hpkg"
"$PACKAGE" create -q -C "$STAGE" "$FILE"
"$PACKAGE" list -i "$FILE"
echo "Package: $FILE"
echo "Staging directory retained for inspection: $STAGE"
