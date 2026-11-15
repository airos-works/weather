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

## Raspberry Pi 4 / Waveshare DSI

After receiving the updated MCP connection details, hardware validation passed on
the actual Raspberry Pi 4 (air/OS arm64, hrev60207+2692):

- Installed the ARM64 `.hpkg` through the native package manager. Its plan touched
  only Weather. AirTop was quit normally to release the screen; boot settings and
  drivers were left unchanged.
- The driver identifies **Waveshare 3.5-inch DSI LCD (E), 640×480**, RGB32,
  2560-byte stride, two 1,228,800-byte buffers, approximately 60 Hz.
- `Weather --panel` fetched live Hobart weather. Scripting reported `valid=true`,
  `stale=false`, `panel=true`, the correct display name and advancing frame counts
  (3 initially, 25 after workflow exercises).
- Captured the driver's **currently shown buffer**, using its read-only dump
  operation, and decoded the BGRA pixels into
  [weather-pi-dsi.png](screenshots/weather-pi-dsi.png). Inspected the 640×480 result:
  large temperature and condition, daily range/feels-like, and one precipitation
  summary, without the desktop charts and tables.
- Ran the same native settings/render tests and live workflow tests on the Pi.
  Both passed. This includes 168-hour forecast retrieval, ten city search matches,
  adding/switching/removing locations, unit changes, rapid selection, refresh
  limiting, corrupt-file preservation, and stale-data rendering.
- Found and fixed the panel-only application's handling of `B_SILENT_RELAUNCH`.
  Relaunch through the registrar then opened one desktop window; closing it left
  zero desktop windows with `panel=true` and advancing frames.
- While Weather was presenting, driver flags were `0x19` (including the powered-on
  bit). After a normal Quit, the job exited with status 0 and flags returned to
  `0x09`, confirming panel power-off.

The hardware tests used `/tmp/weather-dsi-validation` for app settings, preserving
first-launch consent in the normal user settings. No live IP-location request was
made on the user's behalf during these tests.

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
