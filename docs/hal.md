# Hardware abstraction layer (HAL)

Firmware talks **only** to the HAL. It never includes SDL, stdio NVM paths, or LoRa driver APIs. The contract is `hal/hal.h`. `hal/emu` is the RAM HAL for tests and the emulator. `hal/beeper` is the beeper board; `beeper_hal.c` there is a stub that must compile (`make nrf-check`). A real nRF52840 + SX1262 board fills the same `hal_t` function table.

See also: [architecture](architecture.md), [firmware app](firmware-app.md), [hardware](hardware.md), [radio protocol](radio-protocol.md).

---

## What the HAL is here

A **function table plus context pointer**. C firmware calls `hal->foo(hal->ctx, ...)`. Firmware **polls**. The host (or a future RTOS tick) calls `beeper_tick`; that tick asks the HAL “anything new?” and returns.

```
  Emulator timer / test harness        beeper board tick
            |                                |
            v                                v
        hal/emu                           hal/beeper
            \                                /
             \------ hal_t / hal/hal.h -----/
                          |
                    firmware/src
              (beeper, pager, page, pair, store)
```

Host-only hooks (`emu_hal_push_encoder`, `emu_hal_push_raw`, `on_tx_raw`, …) are **not** HAL. Firmware cannot see them.

---

## `hal_t` verbs

| C (`hal_t`) | Role |
|---|---|
| `display_clear` / `display_set_pixel` / `display_buffer` / `display_flush` | ST7567 framebuffer |
| `encoder_poll` / `back_poll` | Queued edges |
| `radio_send_raw` / `radio_poll_raw` | Encrypted PAGE / ACK and pairing |
| `radio_set_whisper` / `radio_whisper` | Pairing TX power flag |
| `random_fill` | Entropy |
| `nvm_load` / `nvm_save` | Opaque blob ≤ 100 KB |
| `now_ms` / `clock` | Monotonic ms; wall HH:MM |
| `battery_pct` / `vibe` / `beep` | Menu footer and haptics |
| `gps_fix` | Latest GPS fix while the receiver is on, or no fix |
| `gps_power` | Drive the GPS enable pin |
| `heading` | Tilt, stillness, motor, and degrees clockwise from north |

### Shared constants (`hal.h`)

| Constant | Value |
|---|---|
| `DISPLAY_WIDTH` / `HEIGHT` / `PAGES` | 128 / 64 / 8 |
| `DISPLAY_BUF_LEN` | 1024 |
| `RAW_FRAME_MAX` | 128 |
| `NVM_BYTES` | `100 * 1024` |
| `PAGE_FROM_MAX` / `PAGE_BODY_MAX` | 15 / 79 |

Index is `(y >> 3) * 128 + x`, bit `1 << (y & 7)`.

---

## Two HALs in-tree

**RAM HAL** (`hal/emu`): queues for encoder, back, and raw RX, an NVM blob, LCG `random_fill`, `on_tx_raw` for the desktop mesh. Used by `make test` and `make emu`.

**Beeper stub** (`hal/beeper`): every `hal_t` function is a no-op or a static display buffer. `random_fill` is incremental. `now_ms` is 0. `gps_fix` returns the last RMC sentence fed to `beeper_gps_push_byte` (the seam for the Feather UART). Enough to `make nrf-check`.

### GPS fix

`gps_power(true)` turns the receiver on. `gps_fix` then writes a `gps_fix_t`: `valid`, and signed arcminutes times 10,000 (`lat_min_e4` north, `lon_min_e4` east). That is the PA1616S RMC field (`ddmm.mmmm` / `dddmm.mmmm`), not decimal degrees. Firmware never sees the NMEA string. `gps_power(false)` makes `gps_fix` return no fix. The RAM HAL starts powered. Firmware turns it off after a sample.

The RAM HAL stores one fix (`emu_hal_set_gps`). The emulator turns an east/north foot grid into that fix. The nRF stub parses `$GPRMC` / `$GNRMC` in `firmware/src/nmea.c` when bytes arrive. Status `A` is a fix. Status `V` is no fix.

### Heading

`heading` writes a `heading_sample_t`: `tilt_deg` off flat, `still` when the accelerometer reads about 1 g, `motor` while a vibe is in progress, and `heading_deg` clockwise from north (`0` = the top of the device points north). It returns false when there is no sample. Firmware (`heading_view`) hides the arrow above 20° of tilt, while moving, and while the motor is running, then waits 400 ms before drawing an 8-way arrow.

The RAM HAL starts flat, still, and facing north (`emu_hal_set_heading`). A vibe stays `motor` until `now_ms` passes the duration. The nRF stub returns false until the LSM303AGR driver exists.

---

## Tick architecture

Each `beeper_tick`:

1. Loads NVM once (`store_load`).
2. Ensures a `node_id` (`random_fill` if the store was empty).
3. If pairing: `pair_tick`. Else: `beeper_sessions_tick`.
4. `pager_poll_send`.
5. If no new page this tick: `pager_input`.
6. `pager_render` → maybe `display_flush`.
7. `store_sync`.

Firmware schedules work with `now_ms()` deadlines. It must **return** and wait for the next tick.

---

## Radio

`radio_send_raw` copies (truncates to `RAW_FRAME_MAX`) and transmits. No loopback. The RAM HAL then calls `on_tx_raw` if the air mesh attached it. `firmware/host/mesh.c` fans TX out to in-range peers with `emu_hal_push_raw`. Range is host-only.

---

## Build

```
make test
make emu
make nrf-check
```
