# PIDKiln architecture notes

## Control path

- `Program_Loop` (FreeRTOS, core 0) advances the kiln FSM at ~1 Hz via a hardware timer semaphore.
- Pure control/safety math lives in `lib/PIDKilnLogic` and is covered by `pio test -e native`.
- HTTP/API read kiln state through `Kiln_ReadTelemetry()` (spinlock snapshot); they do not poke PID globals directly for display values.

## Web / LCD modules

| Module | Role |
|--------|------|
| `PIDKiln_http.cpp` | HTML UI routes, prefs, indexes, chart/vars JSON |
| `PIDKiln_http_programs.cpp` | `/load.html`, `/delete.html` |
| `PIDKiln_http_ota.cpp` | Firmware OTA + optional LCD screenshot |
| `PIDKiln_api.cpp` | `/api/v1` JSON peer API |
| `PIDKiln_lcd.cpp` | Main views, menu, prefs, about, helpers |
| `PIDKiln_lcd_programs.cpp` | Program list / summary / delete / quick program |

## Auth

- HTTP Basic via `WebHost_Auth`. Empty `Auth_Password` disables auth (first-boot); set a password before LAN exposure.
- Shipped `data/etc/pidkiln.conf` defaults leave auth and SoftAP passwords blank.

## Arduino-ESP32 4.x validation checklist

Not validated yet. When bumping the pioarduino platform zip in `platformio.ini`:

1. `pio run -e esp32-wrover-core` and `esp32-wrover-release`
2. Retest AsyncTCP / ESPAsyncWebServer bring-up and HTTP Basic
3. Confirm 1 Hz timer API (`timerBegin` / ISR) still matches Arduino-ESP32 4.x
4. SoftAP + STA reconnect paths
5. Web OTA + `uploadfs` / Wokwi merged image
6. WDT / long kiln run stability
7. `pio test -e native` still green

## Future product surface (not implemented)

These would need new `PK_FEATURE_*` flags, env wiring, and docs — reserved for a later release:

- **MQTT** telemetry / run commands for Home Assistant or Node-RED
- **Home Assistant** MQTT discovery or REST sensor packaging
- HTTPS / signed OTA (larger lift on the Async stack)

Prefer feature-flagged modules matching EMR/SSR2 style rather than baking into every build.
