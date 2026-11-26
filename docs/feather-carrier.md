# Feather carrier

Schematic: `hardware/feather-carrier/beeper.kicad_sch`. This is the breadboard made into a board. It is not the ESP32 layout in `hardware/esp32/`. The Feather image is `hal/feather` (`make feather`).

Rev A keeps the Feather, the Waveshare radio, the BuyDisplay glass, and the Ultimate GPS as modules. The compass is the bare LSM303AGRTR.

U1 3V is the only 3.3 V rail. USB on the Feather is still how the pack charges and how the UF2 image is copied. A6 is the divider inside the Feather. Do not add a second one.

J1 is the only pack. It feeds Feather BAT. Leave the JST on the Feather empty.

U2 pins VBUS, VSYS, 3V3_EN, GP20, and GP22 stay open. The antenna-switch blob stays on A. Fit the antenna before power.

U3 pins 1-3 stay open. Pin 4 is held at 3.3 V so an optional font chip stays off the SPI wires. LEDA is the backlight anode. The cathode is on VSS inside the module, so the switch is high-side.

Backlight polarity matches the firmware. LCD_BL high, including the internal pull-up, turns the light on. Driving LCD_BL low turns it off. R1 keeps the light off until that pull-up is enabled. Q1 only pulls the 100k gate of Q2, so the weak pull-up is enough.

R3 keeps the motor off while A5 floats. Firmware holds A5 low and drives it high for a vibe.

BEEP is Feather D2, which is also NFC2. Firmware toggles D2 at 4 kHz.

LSM303AGR addresses are fixed. R5 and R6 hold CS_XL and CS_MAG high, which is what puts the accelerometer at 0x19 and the magnetometer at 0x1E. C1 is the datasheet 220 nF. R7 and R8 replace the pull-ups that were on the breakout.

Place the 915 MHz antenna at one end of the board and the GPS antenna at the other. Place U5 with the GPS, away from M1. The motor is a permanent magnet, and sleeping it does not remove the field. The breadboard heading offsets do not carry onto this board.
