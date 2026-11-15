#!/usr/bin/env bash
set -euo pipefail
BUILD=${1:-build}
DEST="$HOME/config/non-packaged/apps/Weather"
mkdir -p "$DEST" "$HOME/config/non-packaged/data/deskbar/menu/Applications"
cp "$BUILD/Weather" "$DEST/Weather.new"
chmod 755 "$DEST/Weather.new"
cp -R resources "$DEST/"
mv "$DEST/Weather.new" "$DEST/Weather"
ln -sfn "$DEST/Weather" "$HOME/config/non-packaged/data/deskbar/menu/Applications/Weather"
mimeset -f "$DEST/Weather"
