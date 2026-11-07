# PCB bill of materials

Parts for the compact production board, ten units. Prices are catalog prices at quantity 10 on 1 November 2026, rounded to the cent. Shipping, tariffs, and the bare boards are not in the total.

The sheet in `hardware/beeper` is the Feather carrier and still matches the breadboard. This list is the later board. Reference designators match [PCB connections](pcb-connections.md).

One cell. A buck makes the 3.3 V rail for the ESP32, the radio, the screen, the GPS, the compass, the backlight, the motor, and the piezo. USB-C feeds the charger only. The module supply is 3.0–3.6 V, so USB 5 V stays off that rail.

## Quoted parts

One of each per board.

| Role | Part | Order number | Vendor | Each | Ten |
|---|---|---|---|---:|---:|
| MCU | Espressif ESP32-S3-WROOM-1-N8 | C2913198 | LCSC | $4.78 | $47.80 |
| USB-C jack | GCT USB4105-GF-A | 2073-USB4105-GF-ACT-ND | DigiKey | $0.68 | $6.80 |
| LiPo charger | Microchip MCP73831T-2ACI/OT | MCP73831T-2ACI/OTCT-ND | DigiKey | $0.70 | $7.00 |
| Radio, 902–928 MHz | Heltec HT-RA62 | ZC-144-915 | [Heltec](https://heltec.org/project/ht-ra62/) | $3.90 | $39.00 |
| GPS, patch on the module | Quectel LC86LICMD | C7431213 | LCSC | $7.53 | $75.30 |
| Compass | ST LSM303AGRTR | C126671 | LCSC | $1.94 | $19.40 |
| Screen, 1.7 inch FPC | EastRising ERC12864FSF-9 | ERC12864FSF-9 | BuyDisplay | $2.12 | $21.20 |
| Screen socket | 14-pin 1.0 mm ZIF | — | BuyDisplay | $0.16 | $1.60 |
| Cell, 2000 mAh | LaskaKit 654060 / LA123150 | LA123150 | LaskaKit | $7.25 | $72.50 |
| 915 MHz antenna | DreamLNK T1-915M | C726086 | LCSC | $0.24 | $2.40 |
| Encoder | Bourns PEC11R-4220F-S0024 | PEC11R-4220F-S0024-ND | DigiKey | $1.79 | $17.90 |
| 3.3 V buck | TI TLV62568DBVT | C2864104 | LCSC | $0.10 | $1.00 |
| Battery header | JST S2B-PH-SM4-TB(LF)(SN) | 455-1749-1-ND | DigiKey | $0.42 | $4.20 |
| Piezo, 4 kHz | INGHAi GSC1030RB-3V4000 | C255330 | LCSC | $0.49 | $4.90 |
| Coin motor | Leader LCM1027A2445F | C2759984 | LCSC | $0.49 | $4.90 |
| Passives | Listed in the next section | — | — | $2.50 | $25.00 |
| **Parts** | | | | **$35.09** | **$350.90** |

The $2.50 line is an estimate. Every other line is a listed price. The Heltec price is the single-unit price; that page had no ten-piece break. The cell is €6.49 including VAT, taken as $7.25 at 1.117 dollars per euro. LaskaKit had 44 in stock. Whether that cell ships to the US was not confirmed.

Bare boards are extra. Ten 4-layer boards near 60 × 70 mm are about $40. The cell is 60 × 40 × 6.5 mm, so the board follows the cell rather than a 50 × 70 mm outline.

## Passives and discretes

These are inside the $2.50 estimate. Refs and values match [PCB connections](pcb-connections.md). One of each ref per board.

| Qty | Ref | Value | Role |
|---:|---|---|---|
| 2 | R17, R18 | 5.1 kΩ | USB-C CC1 and CC2 to ground |
| 2 | R14, R15 | 100 kΩ | Cell divider, half the pack onto IO4 |
| 1 | C14 | 100 nF | Across the ADC tap of that divider |
| 1 | R13 | 2.5 kΩ | Charger PROG to ground. Sets 400 mA |
| 2 | C9, C10 | 4.7 µF | Charger: VBUS input and BAT output |
| 1 | L1 | 2.2 µH | Buck inductor |
| 1 | R1 | 100 kΩ, 1% | Buck feedback, top |
| 1 | R2 | 22.1 kΩ, 1% | Buck feedback, bottom. 0.6 V reference gives about 3.31 V |
| 2 | C1, C2 | 10 µF | Buck input (BAT) and output (+3V3) |
| 1 | C8 | 10 µF | +3V3 rail bulk, beside the radio |
| 1 | R16 | 10 kΩ | ESP32 EN pull-up to +3V3 |
| 1 | R20 | 10 kΩ | ESP32 IO3 pull-down so USB boot stays selected |
| 1 | C6 | 100 nF | Radio local bypass |
| 1 | Q4 | AO3401A | GPS high side. VCC and V_BCKP on GPS_VCC. A low on IO39 turns the module on |
| 1 | R6 | 100 kΩ | GPS P-FET gate pull-up to +3V3 |
| 1 | R21 | 1 kΩ | GPS RXD series from IO38 |
| 1 | R22 | 5.6 kΩ | GPS RXD to ground. Divider holds the 2.8 V input under 3.1 V |
| 1 | Q6 | 2N7002 | GPS RESET_N open-drain. A high on IO40 holds reset low |
| 1 | R19 | 100 kΩ | GPS reset gate pulldown |
| 1 | C11 | 4.7 µF | GPS_VCC bulk |
| 1 | C12 | 100 nF | GPS_VCC bypass |
| 1 | C13 | 33 pF | GPS_VCC, closest to the module |
| 2 | R9, R10 | 4.7 kΩ | I2C pull-ups to +3V3 |
| 1 | C3 | 220 nF | LSM303AGR C1 to ground |
| 2 | C4, C5 | 100 nF | Compass supplies |
| 1 | Q1 | BSS138 | Backlight low side on BLK. A high on IO35 turns the lamp on. BLA is tied to +3V3 |
| 1 | R3 | 100 kΩ | Backlight gate pulldown |
| 1 | C7 | 100 nF | Screen supply bypass |
| 1 | SW2 | Tactile, SPST | Back. Active low, internal pull-up only, common to ground |
| 1 | Q5 | 2N7002 | Motor low side. A high on IO16 runs the motor |
| 1 | R7 | 100 kΩ | Motor gate pulldown |
| 1 | D1 | 1N4148, SOD-123 | Motor flyback. Cathode at +3V3, anode at the motor negative |
| 1 | R8 | 100 Ω | Piezo series |

The encoder and the back button have no external resistors. The screen LED anode has no series resistor; the glass has its own.

Footprints already used on the carrier for these classes: resistors and the small capacitors 0603, the 10 µF and 4.7 µF capacitors 0805, the MOSFETs SOT-23, the diode SOD-123, the battery header JST PH horizontal.

## What each module requires

**ESP32-S3-WROOM-1-N8.** 25.5 × 18 mm, PCB antenna, 8 MB flash, 36 GPIO pads. Pin map is in [PCB connections](pcb-connections.md). Leave IO0, IO45, and IO46 open. IO3 is pulled down with R20 so USB programming stays selected. USB is GPIO19 and GPIO20, wired to the USB-C jack, and that port is how the first image goes on. EN is pulled up with R16 and must not float. The antenna end of the module stays clear of the 915 MHz spring and the GPS patch. The chip draws more while it is awake than the nRF52840 on the breadboard. The PCB image is a later firmware, separate from the Feather image and the emulator.

**HT-RA62.** 16 × 17 × 0.8 mm. SPI, busy, and reset, same contract as the breadboard radio. DIO2 inside the module steers the antenna switch. Firmware does not drive a separate antenna GPIO. TX stays at +14 dBm for a page and −9 dBm for whisper. The spring solders to the antenna stamp hole. A stock Wio-SX1262 keeps RF on its IPEX connector, so this spring does not fit that module.

**T1-915M.** 17.5 × 5.5 mm through-hole spring. Minimum buy is 5. Place it at the opposite end of the board from the GPS patch and the compass. The motor is a permanent magnet and sits at the antenna end.

**LC86LICMD.** 16 × 16 × 6.95 mm, with a 15 × 15 × 4 mm patch. UART NMEA at 9600, `$GPRMC` / `$GNRMC`. It has no shutdown pin. A low on IO39 turns Q4 on and powers GPS_VCC. The breadboard drives its enable high, so the PCB firmware inverts that one pin. RXD is a 2.8 V input, so the UART line into the module is R21 / R22 (1 kΩ / 5.6 kΩ). The switched rail at the module has C11, C12, and C13 (4.7 µF, 100 nF, 33 pF) to ground. Backup power is on that same switched rail, so there is no coin cell. RESET_N is the open drain of Q6 on IO40; some modules need reset held low through each power cycle. The patch faces the sky, with about 30 × 30 mm of ground under it, the feed-point keep-out clear, and a few millimeters of plastic over the patch. Keep it away from the spring and the motor.

**LSM303AGRTR.** LGA-12, 2 × 2 mm. Addresses stay 0x19 and 0x1E. Both chip-select pins are tied to 3.3 V, which holds the part in I2C mode. Interrupts stay open. The breadboard offsets (add 275 to X, add 45 to Y, subtract 21°) were measured on that breadboard and do not transfer.

**ERC12864FSF-9.** FPC glass, 43.50 × 33.60 × 3.0 mm, ST7567, 128 × 64, page buffer 1024 bytes. Bias `0xA2`, ratio `0x22`, and volume `0x34` are the current glass settings and may move on this panel. Backlight on is a high on the backlight pin. Font-chip pins stay unused. The header module ERM12864FSF-9 is the taller part and is not this line.

**654060.** JST PH 2.0, protection at 4.26 V and cutoff at 3.0 V. Standard charge is 400 mA, set by the 2.5 kΩ on the charger PROG pin. Two 100 kΩ resistors put half the cell on an ADC pin. The percent and the minutes-to-full belong to the PCB firmware.

**PEC11R-4220F-S0024.** 24 pulses per revolution, detents, built-in switch, vertical, 6 mm flatted shaft. This is the price pick. The knob, the shaft length, and which edge it leaves are still open. Firmware only needs quadrature plus a press.

**TLV62568.** SOT-23-5, 1 A, 1.5 MHz. The cell spans about 3.0–4.2 V, and the radio's rated power wants 3.3 V, so the rail is a buck. 1 A covers a +14 dBm transmit plus the backlight and the GPS. The enable pin can stay tied on.

**GSC1030RB-3V4000.** 10 × 10 mm, passive, resonant at 4 kHz. The piezo pin toggles at 4 kHz. Any free GPIO works, other than the four strapping pins.

**LCM1027A2445F.** 10 mm coin, 3.0 V rated, 2.7–3.3 V operating, 80 mA maximum. A high on the motor gate runs it.

## If a line will not ship

| Swap | Instead | Each | What changes |
|---|---|---:|---|
| Cell | EEMB LP103454-PCM, Amazon 10-pack $82.99 | $8.30 | 56 × 34.5 × 10.3 mm, JST PH, protection, same 400 mA standard charge. About $1.05 more, and thicker |
| GPS | ATGM336H-5NR32-G (LCSC C49233943, $1.68) plus Bat Wireless BWGPSCNX8-8B1 (LCSC C496567, $1.59) | $3.27 | Saves about $4.26. Needs a board IPEX jack and a 47 nH from the module's RF bias pin. An 8 mm patch in a pocket can miss the 60 second listen |
| Screen | EastRising ERC12864FSF-11, 0.96 inch | $1.28 | Same ST7567 SPI. Outline 27.30 × 21.50 × 3.3 mm, backlight about 20 mA. Only if the pager gets smaller |

## Related

- [PCB connections](pcb-connections.md) — nets and pins this list follows
- [Hardware](hardware.md) — why each function exists, and the Feather pin contract
- [Prototype assembly](prototype-assembly.md) — the breadboard this board replaces
