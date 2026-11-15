# Validation — 15 November 2026

The desktop application has been exercised on an x86_64 air/OS workstation
running Haiku hrev60207+2712. ARM64 builds use the existing Raspberry Pi 4 air/OS
cross-toolchain and the Pi build's `libdevice` with `BAuxDisplay` support.

## Passing checks

- `make check-host`: portable forecast parsing and formatting with AddressSanitizer
  and UndefinedBehaviorSanitizer. Covers a real Open-Meteo fixture, missing/null
  fields, invalid coordinates, URL encoding, timezone conversion, unit conversion,
  city disambiguation, approximate-location response parsing, precipitation summaries,
  missing precipitation and stale forecasts.
- Native application build and `build/native_test --live`: real HTTPS forecast
  fetch (168 hourly entries, seven days), real city search (ten matches), settings
  round-trip, corrupt settings preservation, and native rendering.
- Desktop rendering at 980×840 and panel rendering at 800×480 and 480×320,
  including stale data. The desktop and 800×480 images have been visually inspected.
- First-launch permission dialog was shown. Choosing **Choose city** left consent
  false, zero location requests and zero forecast requests. Searching for Hobart
  returned city/region/country results without making an IP-location request.
- `build/workflow_test` against a disposable application settings directory:
  live search and addition of London, saved-city switching, rapid selection while
  requests are in flight, Celsius/Fahrenheit switching, removal, repeat-refresh
  limiting, and permission revocation. IP-location request count remained zero.
- Restarting the disposable instance retained its chosen city and denied/revoked
  permission without asking again. The cached forecast was loaded and refreshed.
- ARM64 application cross-build links successfully against the actual auxiliary
  display library. All artwork is bundled and loads through the Translation Kit.
- Native install and Deskbar symlink verified; installed binary SHA-256 matches
  the tested build. Beta packages were generated and their architecture/version
  metadata inspected for both x86_64 and ARM64.
- The installed executable was launched with `--panel` on a workstation without
  an auxiliary device. It opened its desktop window, fetched live weather and
  reported `No available auxiliary display` with `panel=false`, without crashing.

The live tests deliberately use Hobart and London as test locations; they do not
infer the operator's location. The IP-location success response is covered by
parser tests; live permission approval has not been granted on the user's behalf.

## Outstanding hardware verification

The physical Raspberry Pi 4 / Waveshare DSI path remains **unverified**. The known
network and console access methods were unavailable during this session. Native
offscreen rendering and a successful ARM64 link do not prove presentation on the
actual panel.

Once the Pi is reachable:

1. Build/package the current source with `tools/build-arm64.sh` and `tools/package.sh`.
2. Install Weather on the Pi, preserving its existing apps and startup settings.
3. Confirm the DSI device and its dimensions. Release AirTop's auxiliary display
   before launching Weather, without changing unrelated system services.
4. Configure an explicit test city or let the user grant location permission.
5. Run `Weather --panel`, verify live data and increasing `panel_frames` via
   `hey Weather get WeatherState`, and inspect the physical panel/captured buffer.
6. Verify large text at the panel's actual dimensions, window closure with the panel
   continuing, desktop relaunch, location switching, stale presentation, and display
   power-off when the panel is disabled or Weather quits.

The complete goal should remain open until these checks pass.

## Repeating native workflow tests

Quit any ordinary Weather instance before using the test signature. Launch a
disposable instance, decline location permission if prompted, then select the
explicit test city:

```sh
WEATHER_SETTINGS_DIR=/tmp/weather-workflow ./build/Weather --location -42.88 147.33 'Hobart, Tasmania'
# In another Terminal:
make build/workflow_test
build/workflow_test
hey Weather get WeatherState
```

The workflow test refuses to run without `WEATHER_SETTINGS_DIR` in the running
app. It expects one saved Hobart location and no location permission. It leaves
one saved Hobart location, Celsius units, and permission revoked. Native settings
tests use their own temporary directory and preserve simulated corrupt data until
the test explicitly cleans it up.
