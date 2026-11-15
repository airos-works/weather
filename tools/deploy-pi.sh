#!/usr/bin/env bash
# Copy a built ARM64 app to an SSH-accessible air/OS Pi. Does not stop AirTop.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
: "${WEATHER_PI_HOST:?Set WEATHER_PI_HOST to an SSH host or user@address}"
SSH=(ssh)
if [[ -n ${WEATHER_SSH_CONFIG:-} ]]; then SSH+=(-F "$WEATHER_SSH_CONFIG"); fi
test -f "$ROOT/build-arm64/Weather"
tar -C "$ROOT/build-arm64" -cf - Weather resources | "${SSH[@]}" "$WEATHER_PI_HOST" \
    'mkdir -p "$HOME/config/non-packaged/apps/Weather" && cd "$HOME/config/non-packaged/apps/Weather" && tar -xm -f - && chmod 755 Weather && mkdir -p "$HOME/config/non-packaged/data/deskbar/menu/Applications" && ln -sfn "$HOME/config/non-packaged/apps/Weather/Weather" "$HOME/config/non-packaged/data/deskbar/menu/Applications/Weather" && mimeset -f Weather'
echo 'Installed. Configure a location, release the display from AirTop, then run Weather --panel on the Pi.'
