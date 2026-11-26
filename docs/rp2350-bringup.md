# RP2350 bringup

Work for the v3 board (`hardware/rp2350/peeple.kicad_sch`), built by hand with a hotplate and an iron. Parts are in [rp2350-bom.md](rp2350-bom.md). Pins are in [rp2350-connections.md](rp2350-connections.md). There is no v3 layout and no RP2350 firmware image yet. Add the next item here when it comes up.

## Assembly

The board is drawn for one hotplate pass on the top side and an iron for the rest.

1. Paste the whole top side through a stencil. U1 (QFN-60, 0.4 mm pitch), U6 (LGA-12), and U7 (LGA-16) cannot be done with an iron. Everything else can, but pasting it all at once is faster.
2. Place every SMD part. L3 has a polarity dot; it goes on the VREG_LX side. Check the orientation of D1, D2, D3, U2, U8, U9, and the FETs against the silkscreen.
3. Reflow on the hotplate. Let it cool on the plate.
4. Look at U1 under magnification. Clear bridges with flux and wick. Reflow U6 and U7 with hot air or the plate again if they look tilted; there is no way to touch them up with an iron.
5. With the iron: SW1 (encoder, through-hole), AE1 (spring antenna), M1 leads, and J4 if you want SWD. Anything that missed on the plate.
6. Plug the GPS patch into J5 and the glass into J3 last.

## First power

Before any power, measure resistance to `GND` on `VSYS`, `+3V3`, and `+1V1`. None of them should read as a short.

1. No cell. USB through a USB meter or from a bench supply limited to about 200 mA. `+3V3` should read 3.30–3.35 V. `+1V1` should read about 1.1 V. Idle current should be tens of milliamps.
2. Hold BOOTSEL (SW3) while plugging USB, or hold BOOTSEL and tap RESET (SW4). A drive named `RP2350` appears. Copy a blink UF2 and confirm it runs.
3. Unplug. Check J2 polarity against the cell (pin 1 positive). Plug the cell. `+3V3` comes up from the cell through Q3.
4. Plug USB with the cell in. D3 lights while the cell charges. `VSYS` moves from the cell to about 4.7 V (USB minus the B5819W drop).
5. GPIO27 reads about 3.0 V with USB present and 0 V without. GPIO26 reads half the cell.

## Firmware port

The image goes in `hal/rp2350/` as `rp2350_hal.c` plus a board file, and implements `hal/hal.h` like `hal/feather`. Use the Pico SDK for an RP2350 with 2 MB of flash.

- **Radio.** HT-RA62, an SX1262 like the Feather's. Reuse `hal/feather/sx1262_setup.c` after checking that the module's TCXO also wants 1.8 V on DIO3. SPI1 on GPIO12 (MISO), GPIO14 (SCK), GPIO15 (MOSI). CS GPIO13, RESET GPIO16, BUSY GPIO17. DIO1 on GPIO18 is optional; BUSY polling still works.
- **Screen.** ST7567 on the same SPI1. CS GPIO19, A0 GPIO20, RESET GPIO21. Backlight is a high on GPIO22.
- **Controls.** Encoder A / B on GPIO6 / GPIO7, push on GPIO8, Back on GPIO9. All active low; enable the internal pull-ups. PEC11R, 24 pulses and 24 detents, one step per detent.
- **Accelerometer.** SC7A20H at 0x19. Same register map as the LSM303AGR accelerometer the Feather reads: CTRL_REG1 0x20, CTRL_REG4 0x23, data from 0x28 with the auto-increment bit. Only WHO_AM_I (0x0F) differs: 0x11 instead of 0x33.
- **Magnetometer.** QMC5883P at 0x2C is a new driver. Chip ID at 0x00 reads 0x80. Setup from the datasheet: 0x29 ← 0x06 (axis signs), 0x0B ← 0x08 (set/reset on, ±8 G), 0x0A ← 0xCD (normal mode, 200 Hz). Status bit 0 of 0x09 is data ready. X, Y, Z are at 0x01–0x06, LSB first, signed 16-bit.
- **GPS.** UART0 at 9600: GPIO0 to RXD, GPIO1 from TXD. The ATGM336H sends `$GNRMC`; `nmea.c` already matches any talker. A low on GPIO2 powers it, so the Feather's enable logic inverts. GPIO3 is reset: leave it an input and pull it low only to reset. Never drive it high. Set GPIO0 to an input while the GPS is off so U1 does not feed the module through RXD.
- **Battery.** GPIO26 (ADC0) is half the cell. GPIO27 (ADC1) is USB present. Charge is 500 mA into 2000 mAh, not the Feather's 200 mA into 1200 mAh.
- **Haptics.** Motor is a high on GPIO10. Piezo is GPIO11, PWM slice 5 channel B, 4 kHz.
- **Store.** The 100 KB NVM blob goes in the last sectors of the 2 MB flash.
- **Random.** The RP2350 has a hardware TRNG.

Spare pins: GPIO23–25, GPIO28, GPIO29.

## After it boots

- [ ] Calibrate the magnetometer for this board. Breadboard and v2 offsets do not transfer.
- [ ] Calibrate true north, south, east, and west.
- [ ] Force a sky wait on first boot so the receiver can take its first satellite fix.
- [ ] Check that the heading holds steady while the motor runs, and that the 400 ms wait after a vibe is still long enough.
