# RP2350 bill of materials

Parts for the v3 board, ten units, built by hand with a hotplate and an iron. Prices are the LCSC, DigiKey, Heltec, and BuyDisplay product pages on 25 November 2026, at the price break the ten-board order reaches, rounded to the cent. Shipping, tariffs, the stencil, and the bare boards are not in the total.

Reference designators match [RP2350 connections](rp2350-connections.md) and `hardware/rp2350/peeple.kicad_sch`. The v2 ESP32 list is [esp32-bom.md](esp32-bom.md). There is no RP2350 firmware image in this tree yet; what the port needs is in [rp2350-bringup.md](rp2350-bringup.md).

One cell. USB and the cell share `VSYS` (Schottky D2 from USB, P-FET Q3 from the cell). One TLV62568 makes `+3V3` from `VSYS`. That rail feeds the RP2354A, radio, screen, GPS (switched), sensors, backlight, motor, and piezo. The RP2354A makes its own 1.1 V core rail with L3.

## Quoted parts

One of each ref per board unless the Ref column lists more. New spec PDFs are in [`rp2350-datasheets/`](rp2350-datasheets/). Parts carried over from v2 link to [`esp32-datasheets/`](esp32-datasheets/).

| Ref | Role | Part | Order number | Vendor | Spec | Each | Ten |
|---|---|---|---|---|---|---:|---:|
| U1 | MCU, 2 MB flash inside | Raspberry Pi RP2354A | C41378174 | LCSC | [PDF](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf) | $1.24 | $12.38 |
| Y1 | 12 MHz crystal | Abracon ABM8-272-T3 | C20625731 | LCSC | [guide](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf) | $0.40 | $4.00 |
| L3 | Core regulator inductor, 3.3 µH | Abracon AOTA-B201610S3R3-101-T | C42411119 | LCSC | [guide](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf) | $0.32 | $3.16 |
| J1 | USB-C jack, 16 pin | HRO TYPE-C-31-M-12 | C165948 | LCSC | [PDF](rp2350-datasheets/TYPE-C-31-M-12.pdf) | $0.17 | $1.74 |
| U8 | USB ESD | USBLC6-2SC6 | C2827654 | LCSC | [PDF](rp2350-datasheets/USBLC6-2SC6.pdf) | $0.05 | $0.45 |
| U2 | Li-ion charger | TPOWER TP4054 | C668215 | LCSC | [PDF](rp2350-datasheets/TP4054.pdf) | $0.04 | $0.39 |
| U3 | 3.3 V buck | TI TLV62568DBVR | C163219 | LCSC | [PDF](esp32-datasheets/TLV62568.pdf) | $0.08 | $0.76 |
| L1 | Buck inductor, 2.2 µH | Changjiang FNR4030S2R2MT | C167869 | LCSC | — | $0.08 | $0.79 |
| D2 | USB to VSYS Schottky | B5819W, SOD-123 | C2943878 | LCSC | — | $0.02 | $0.15 |
| Q2, Q3 | GPS switch, cell to VSYS | AOS AO3401A | C15127 | LCSC | [PDF](https://www.aosmd.com/sites/default/files/res/datasheets/AO3401A.pdf) | $0.17 | $1.65 |
| U4 | Radio, 902–928 MHz | Heltec HT-RA62 | ZC-144-915 | [Heltec](https://heltec.org/project/ht-ra62/) | [PDF](esp32-datasheets/HT-RA62.pdf) | $3.90 | $39.00 |
| AE1 | 915 MHz antenna | DreamLNK T1-915M | C726086 | LCSC | [PDF](esp32-datasheets/DreamLNK-T1-915M.pdf) | $0.24 | $2.39 |
| U5 | GPS module | Zhongke ATGM336H-5NR32-G | C49233943 | LCSC | [PDF](rp2350-datasheets/ATGM336H-5NR32-G.pdf) | $1.68 | $16.75 |
| — | GPS active patch on IPEX cable | Bat Wireless BWGPSCNX8-8B1 | C496567 | LCSC | — | $1.59 | $15.90 |
| J5 | GPS antenna jack | Hirose U.FL-R-SMT-1(80) | C88374 | LCSC | — | $0.10 | $1.04 |
| L4 | GPS antenna bias, 47 nH | Murata LQW18AN47NG00D | C98076 | LCSC | — | $0.05 | $0.52 |
| U6 | Accelerometer | Silan SC7A20HTR | C19274408 | LCSC | [PDF](rp2350-datasheets/SC7A20H.pdf) | $0.22 | $2.16 |
| U7 | Magnetometer | QST QMC5883P | C2847467 | LCSC | [PDF](rp2350-datasheets/QMC5883P.pdf) | $1.86 | $18.60 |
| — | Screen glass, 1.7 inch FPC | EastRising ERC12864FSF-9 | ERC12864FSF-9 | BuyDisplay | [PDF](esp32-datasheets/ERC12864-9_Datasheet.pdf) | $2.12 | $21.20 |
| J3 | Screen socket | Molex 200528-0140 | — | BuyDisplay | [PDF](esp32-datasheets/Molex_200528.pdf) | $0.16 | $1.60 |
| Q1 | Backlight switch | BSS138 | C78284 | LCSC | — | $0.02 | $0.21 |
| SW1 | Encoder with push | Bourns PEC11R-4220F-S0024 | C143797 | LCSC | [PDF](esp32-datasheets/PEC11R.pdf) | $2.33 | $23.30 |
| SW2–SW4 | Back, BOOTSEL, RESET | XKB TS-1187A-B-A-B | C318884 | LCSC | — | $0.06 | $0.59 |
| U9 | Encoder and button ESD | SRV05-4 | C558418 | LCSC | [PDF](rp2350-datasheets/SRV05-4.pdf) | $0.03 | $0.26 |
| M1 | Coin motor | Jie Yi JYC1027 | JYC1027 | DigiKey | — | $1.24 | $12.36 |
| Q4 | Motor switch | AOS AO3400A | C20917 | LCSC | [PDF](https://www.aosmd.com/sites/default/files/res/datasheets/AO3400A.pdf) | $0.08 | $0.79 |
| D1 | Motor flyback | 1N4148W, SOD-123 | C81598 | LCSC | — | $0.01 | $0.12 |
| BZ1 | Piezo, 4 kHz | INGHAi GSC1030RB-3V4000 | C255330 | LCSC | [PDF](esp32-datasheets/GSC1030RB-3V4000.pdf) | $0.49 | $4.86 |
| J2 | Cell header, PH 2.0 | DEALON PH-2PWB | C2905019 | LCSC | [PDF](rp2350-datasheets/PH-2PWB.pdf) | $0.03 | $0.32 |
| D3 | Charge LED, red 0805 | NCD0805R1 | C84256 | LCSC | — | $0.01 | $0.12 |
| J4 | SWD header, optional | 1×40 2.54 mm strip, cut to 3 | C2337 | LCSC | — | $0.02 | $0.16 |
| — | Cell, 2000 mAh | Adafruit 2011, 60 × 36 × 7 mm | 2011 | DigiKey | [page](https://www.digikey.com/en/products/detail/adafruit-industries-llc/2011/6612469) | $12.50 | $125.00 |
| — | Passives | Listed in the next section | — | LCSC | — | $0.69 | $6.92 |
| | **Parts** | | | | | **$31.96** | **$319.64** |

Each and Ten are what the parts that go on the boards cost at the BOM vendor (cell line still shows DigiKey Adafruit for a known-good quote). The live shopping cart uses the AliExpress 103450 pouch at $1.80: [rp2350-shopping-list.md](rp2350-shopping-list.md) is **$224.74** cart ($22.47 / board), or **$208.79** allocated ($20.88 / board) if leftovers are ignored.

v2 was $38.76 a board with the $7.25 LaskaKit cell. That cell is out of stock, and v3 uses the $12.50 Adafruit cell instead. With the same cell, v3 would be $26.71, $12.05 less than v2. Most of it is the GPS ($7.53 module → $3.42 with antenna, jack, and choke), the MCU ($4.78 module → about $2.17 with crystal, inductor, and its passives), and the cheaper connectors, charger, ESD, and power path.

The SWD header line is one 40-pin strip; it covers 13 boards.

There is no v3 layout yet. A board drawn from this schematic will need a stencil for U1 (QFN-60, 0.4 mm pitch), U6 (LGA-12), and U7 (LGA-16).

## Passives

Inside the $0.69 line. Refs and values match [RP2350 connections](rp2350-connections.md). 0805 hand-solder footprints everywhere except the parts that crowd U1, which are 0603.

| Qty | Ref | Value | Size | Order number | Role |
|---:|---|---|---|---|---|
| 2 | R3, R4 | 5.1 kΩ | 0805 | C27834 | USB-C CC1 and CC2 to ground |
| 2 | R19, R20 | 27 Ω | 0603 | C25190 | USB D+ and D− series, next to U1 |
| 1 | R23 | 100 kΩ | 0805 | C149504 | VBUS detect top |
| 1 | R22 | 150 kΩ | 0805 | C17470 | VBUS detect bottom (~3.0 V on GPIO27 at 5 V) |
| 1 | C9 | 100 nF | 0805 | C49678 | Across `VBUS_DET` |
| 2 | C1, C2 | 10 µF, 100 nF | 0805 | C15850, C49678 | VBUS at the jack |
| 1 | R7 | 2 kΩ | 0805 | C17604 | Charger PROG → 500 mA |
| 1 | R8 | 1 kΩ | 0805 | C17513 | Charge LED |
| 2 | C3, C4 | 4.7 µF | 0805 | C1779 | Charger VBUS and BAT |
| 2 | R16, R17 | 100 kΩ | 0805 | C149504 | Cell divider onto GPIO26 |
| 1 | C8 | 100 nF | 0805 | C49678 | Across `BAT_ADC` |
| 1 | R21 | 100 kΩ | 0805 | C149504 | Q3 gate to ground when USB is absent |
| 2 | R5, R6 | 453 kΩ, 100 kΩ | 0805 | C17702, C149504 | Buck feedback → 3.32 V |
| 3 | C5, C6, C7 | 10 µF | 0805 | C15850 | Buck input (`VSYS`) and output (`+3V3`) |
| 1 | R1 | 33 Ω | 0603 | C23140 | VREG_AVDD filter |
| 3 | C21, C22, C23 | 4.7 µF | 0603 | C69335 | VREG_VIN, `+1V1` output, VREG_AVDD |
| 11 | C10–C20 | 100 nF | 0603 | C14663 | One per U1 supply pin (53 and 54 share C16) |
| 1 | R13 | 1 kΩ | 0603 | C21190 | Crystal drive limit on XOUT |
| 2 | C24, C25 | 15 pF | 0603 | C1644 | Crystal load |
| 1 | R2 | 1 kΩ | 0603 | C21190 | BOOTSEL button into QSPI_SS |
| 1 | L2 | 0 Ω | 0805 | C17477 | Antenna link |
| 2 | C26, C27 | 10 µF, 100 nF | 0805 | C15850, C49678 | Radio supply |
| 1 | R9 | 100 kΩ | 0805 | C149504 | `GPS_PWR_N` pull-up (GPS off at reset) |
| 1 | R18 | 1 kΩ | 0805 | C17513 | GPS RXD series |
| 3 | C28–C30 | 10 µF, 100 nF, 100 nF | 0805 | C15850, C49678 | `GPS_VCC` and GPS VBAT |
| 2 | R14, R15 | 4.7 kΩ | 0805 | C17673 | I2C pull-ups |
| 3 | C31–C33 | 100 nF | 0805 | C49678 | SC7A20H VDD and VDDIO, QMC5883P VDD |
| 1 | C34 | 4.7 µF | 0805 | C1779 | QMC5883P C1 reservoir |
| 1 | R10 | 10 kΩ | 0805 | C17414 | Backlight gate pull-down |
| 2 | C35, C36 | 100 nF | 0805 | C49678 | Screen supply |
| 2 | C37, C38 | 100 nF | 0805 | C49678 | BACK and ENC_SW |
| 1 | R11 | 10 kΩ | 0805 | C17414 | Motor gate pull-down |
| 1 | R12 | 100 Ω | 0805 | C17408 | Piezo series |

Capacitors are X7R or X5R, 16 V or more; the two 15 pF are C0G.

## What each part requires

**RP2354A.** QFN-60, 7 × 7 mm, 0.4 mm pitch. It needs a stencil and the hotplate; an iron cannot reach the centre pad. Copy the regulator corner (L3, C21, C22, C23, R1) from the Raspberry Pi minimal design. L3 has a polarity dot that goes on the VREG_LX side. Pin map and decoupling placement are in [RP2350 connections](rp2350-connections.md).

**ABM8-272-T3.** The crystal Raspberry Pi tests with. Load is 10 pF and ESR is 50 Ω or less. With 15 pF caps and R13 = 1 kΩ it matches the Pico 2.

**HT-RA62.** Unchanged from v2. 16 × 17 mm castellated module, SX1262. DIO1 now reaches GPIO18.

**ATGM336H-5NR32-G.** 10.1 × 9.7 mm, 18 castellated pads at 1.1 mm. It uses KiCad's Quectel L76 footprint, which has the same pads. UART at 9600 baud, NMEA. ON/OFF is tied to `GPS_VCC`. The antenna is an active patch on a short IPEX cable, so the patch can sit at the top of the case away from the motor.

**SC7A20H.** LGA-12, 2 × 2 mm. Register map is the LIS2DH one at address 0x19. Only WHO_AM_I differs: 0x11 instead of 0x33.

**QMC5883P.** LGA-16, 3 × 3 mm. Address 0x2C, chip ID 0x80 at register 0x00. C34 on the C1 pin is the 4.7 µF the datasheet asks for. Keep it, the motor, and the cell away from U7.

**TP4054.** SOT-23-5 with the MCP73831 pinout: 1 CHRG, 2 GND, 3 BAT, 4 VCC, 5 PROG. At 500 mA from 5 V it runs warm and folds back on its own thermal limit.

**TLV62568.** SOT-23-5, 1 A, 1.5 MHz, 100 % duty mode. The only 3.3 V regulator on the board.

**AO3401A (Q3).** Drain on `BAT`, source on `VSYS`, gate on `VBUS`. The body diode points from the cell to `VSYS`, so the cell can always carry the load even before the FET turns on.

**PEC11R-4220F-S0024.** Unchanged from v2. 24 pulses, 24 detents, push switch.

**TS-1187A.** 5.1 × 5.1 mm SMD tactile with four legs. Easy with an iron.

**PH-2PWB.** Generic JST PH 2.0 right-angle SMD header. It fits the KiCad JST S2B-PH-SM4-TB footprint within hand-solder tolerance. Check cell polarity before plugging in; pin 1 is positive.

## What changed from v2

| Function | v2 | v3 | Each saved |
|---|---|---|---:|
| MCU | ESP32-S3-WROOM-1-N8 module | RP2354A + crystal + core inductor + 0603 passives | $2.61 |
| GPS | Quectel LC86LICMD, patch on the module | ATGM336H + active patch + U.FL + 47 nH | $4.11 |
| Compass | LSM303AGR | SC7A20H + QMC5883P | −$0.14 (costs more) |
| Charger | MCP73831 (DigiKey) | TP4054 | $0.66 |
| USB-C jack | GCT USB4105-GF-A | HRO TYPE-C-31-M-12 | $0.51 |
| USB ESD | USBLC6-2P6 (SOT-666) | USBLC6-2SC6 (SOT-23-6) | $0.35 |
| Input ESD | Five ESD5Z3.3 | One SRV05-4 | $0.62 |
| Power | Two bucks, LM66100, MBR0530, 2N7002 | One buck, B5819W, AO3401A | about $0.47 |
| Tactiles | Two TL3305 | Three TS-1187A (adds RESET) | $0.36 |
| Cell header | JST S2B-PH-SM4-TB | PH-2PWB | $0.39 |
| Passives | $4.00 estimate | $0.69 itemized | $3.31 |

Kept from v2: radio, antenna, encoder, piezo, and screen. GPS reset is now a direct pin instead of Q6 / R27. The v2 motor (Leader LCM1027A2445F) is no longer sold by LCSC and the LaskaKit cell is out of stock, so v3 uses the Jie Yi JYC1027 (same 10 × 2.7 mm coin, wire leads, 3 V) and the Adafruit 2011 cell. The encoder now comes from LCSC; DigiKey is 7 % cheaper, under the 15 % it takes to split the order.

Three cost-down ideas were checked and dropped. The Ai-Thinker Ra-01SC is 410–525 MHz only, so it cannot replace the 915 MHz HT-RA62. The Alps EC11E152T408 encoder is the push-lock version with no push switch. The FSR-1025 piezo is a wire-lead disc, not an SMD part. Two parts were chosen for hand assembly over price: the QMC5883P (LGA, about $1 more than a 0.8 mm WLP part) and the SOT-23-6 USB ESD.

## If a line will not ship

| Swap | Instead | Each | What changes |
|---|---|---:|---|
| L3 | Same part from DigiKey | — | LCSC had 4,160 on 25 November; only JLC's assembly stock is short. Raspberry Pi asks for this exact part because of its polarity mark; do not substitute an unmarked 3.3 µH |
| Y1 | Epson X322512MMB4SI (C50430) | $0.11 | Same 10 pF load, but 80 Ω ESR against the 50 Ω the guide assumes. Only after checking that it starts cold |
| Radio | Ai-Thinker Ra-01SH (SX1262, 803–930 MHz) | $3.95 | Different pinout and outline. New footprint and symbol |
| Encoder | Same part from DigiKey | $2.17 | Nothing |
| Cell | LaskaKit 654060, €6.47 when back in stock | ~$7.25 | 60 × 40 × 6.5 mm. Ships from the Czech Republic |
| Cell | EEMB LP103454-PCM, Amazon 10-pack $82.99 on 14 November, not rechecked | $8.30 | 56 × 34.5 × 10.3 mm, JST PH, protection. Thicker |

## Related

- [RP2350 connections](rp2350-connections.md) — nets and pins this list follows
- [RP2350 bringup](rp2350-bringup.md) — assembly order and firmware notes
- [ESP32 bill of materials](esp32-bom.md) — the v2 list this replaces
