# ESP32 bill of materials

> v2. The RP2350 board replaces it: [rp2350-bom.md](rp2350-bom.md).

Parts for the compact production board, ten units. Prices are catalog prices at quantity 10 on 14 November 2026, rounded to the cent, unless noted. Shipping, tariffs, and the bare boards are not in the total.

The sheet in [`hardware/feather-carrier`](../hardware/feather-carrier) is the Feather carrier and still matches the breadboard. Notes for that sheet are in [feather-carrier.md](feather-carrier.md). This list is the ESP32-S3 board. Reference designators match [ESP32 connections](esp32-connections.md) and `hardware/esp32/peeple.kicad_sch`. There is no ESP32 firmware image in this tree yet. Breadboard compass offsets do not transfer.

One cell. Two TLV62568 bucks OR onto `+3V3`: U3 from the cell through LM66100 (U10), U9 from USB through Schottky D9. That rail feeds the ESP32, radio, screen, GPS (switched), compass, backlight, motor, and piezo. USB-C also feeds the charger. The module supply is 3.0–3.6 V, so raw USB 5 V stays off the MCU pads.

## Quoted parts

One of each ref per board unless the Role column says otherwise.

Spec PDFs live in [`esp32-datasheets/`](esp32-datasheets/) so the links work offline. New power and ESD parts use manufacturer PDFs linked from the vendor page when no local file is checked in yet.

| Ref | Role | Part | Order number | Vendor | Spec | Each | Ten |
|---|---|---|---|---|---|---:|---:|
| U1 | MCU | Espressif ESP32-S3-WROOM-1-N8 | C2913198 | LCSC | [PDF](esp32-datasheets/ESP32-S3-WROOM-1_datasheet.pdf) | $4.78 | $47.80 |
| J1 | USB-C jack | GCT USB4105-GF-A | 2073-USB4105-GF-ACT-ND | DigiKey | [PDF](esp32-datasheets/USB4105.pdf) | $0.68 | $6.80 |
| U2 | LiPo charger | Microchip MCP73831T-2ACI/OT | MCP73831T-2ACI/OTCT-ND | DigiKey | [PDF](esp32-datasheets/MCP73831.pdf) | $0.70 | $7.00 |
| U8 | USB ESD | ST USBLC6-2P6 | 497-5026-1-ND | DigiKey | [PDF](https://www.st.com/resource/en/datasheet/usblc6-2.pdf) | $0.40 | $4.00 |
| U3 | 3.3 V buck (battery) | TI TLV62568DBVT | C2864104 | LCSC | [PDF](esp32-datasheets/TLV62568.pdf) | $0.10 | $1.00 |
| U9 | 3.3 V buck (USB) | TI TLV62568DBVT | C2864104 | LCSC | [PDF](esp32-datasheets/TLV62568.pdf) | $0.10 | $1.00 |
| U10 | Ideal diode OR | TI LM66100DCKR | 296-53541-1-ND | DigiKey | [PDF](https://www.ti.com/lit/ds/symlink/lm66100.pdf) | $0.25 | $2.50 |
| D9 | USB OR Schottky | onsemi MBR0530T1G | MBR0530T1GOSCT-ND | DigiKey | [PDF](https://www.onsemi.com/pdf/datasheet/mbr0530t1-d.pdf) | $0.19 | $1.90 |
| U4 | Radio, 902–928 MHz | Heltec HT-RA62 | ZC-144-915 | [Heltec](https://heltec.org/project/ht-ra62/) | [PDF](esp32-datasheets/HT-RA62.pdf) | $3.90 | $39.00 |
| U5 | GPS, patch on the module | Quectel LC86LICMD | C7431213 | LCSC | [PDF](esp32-datasheets/Quectel_LC86L_GNSS_Specification.pdf) | $7.53 | $75.30 |
| U6 | Compass | ST LSM303AGRTR | C126671 | LCSC | [PDF](esp32-datasheets/LSM303AGR.pdf) | $1.94 | $19.40 |
| U7 | Screen, 1.7 inch FPC | EastRising ERC12864FSF-9 | ERC12864FSF-9 | BuyDisplay | [PDF](esp32-datasheets/ERC12864-9_Datasheet.pdf) | $2.12 | $21.20 |
| J3 | Screen socket | Molex 200528-0140 | — | BuyDisplay | [PDF](esp32-datasheets/Molex_200528.pdf) | $0.16 | $1.60 |
| — | Cell, 2000 mAh | LaskaKit 654060 / LA123150 | LA123150 | LaskaKit | [page](https://www.laskakit.cz/en/geb-lipol-baterie-654060-2000mah-3-7v-jst-ph-2-0/) | $7.25 | $72.50 |
| AE1 | 915 MHz antenna | DreamLNK T1-915M | C726086 | LCSC | [PDF](esp32-datasheets/DreamLNK-T1-915M.pdf) | $0.24 | $2.40 |
| SW1 | Encoder | Bourns PEC11R-4220F-S0024 | PEC11R-4220F-S0024-ND | DigiKey | [PDF](esp32-datasheets/PEC11R.pdf) | $1.79 | $17.90 |
| SW2 | Back tactile | E-Switch TL3305AF160QG | EG5626CT-ND | DigiKey | [PDF](https://www.e-switch.com/wp-content/uploads/2022/07/TL3305.pdf) | $0.21 | $2.10 |
| SW3 | Boot tactile | E-Switch TL3305AF160QG | EG5626CT-ND | DigiKey | [PDF](https://www.e-switch.com/wp-content/uploads/2022/07/TL3305.pdf) | $0.21 | $2.10 |
| J2 | Battery header | JST S2B-PH-SM4-TB(LF)(SN) | 455-1749-1-ND | DigiKey | [PDF](esp32-datasheets/JST_PH_Series.pdf) | $0.42 | $4.20 |
| BZ1 | Piezo, 4 kHz | INGHAi GSC1030RB-3V4000 | C255330 | LCSC | [PDF](esp32-datasheets/GSC1030RB-3V4000.pdf) | $0.49 | $4.90 |
| M1 | Coin motor | Leader LCM1027A2445F | C2759984 | LCSC | [PDF](esp32-datasheets/LCM1027A2445F.pdf) | $0.49 | $4.90 |
| Q2 | GPS high-side P-FET | AOS AO3401A | C15127 | LCSC | [PDF](https://www.aosmd.com/sites/default/files/res/datasheets/AO3401A.pdf) | $0.08 | $0.80 |
| Q5 | Motor low-side N-FET | AOS AO3400A | C20917 | LCSC | [PDF](https://www.aosmd.com/sites/default/files/res/datasheets/AO3400A.pdf) | $0.08 | $0.80 |
| D2, D4–D7 | ESD, five places | onsemi ESD5Z3.3T1G | ESD5Z3.3T1GOSCT-ND | DigiKey | [PDF](https://www.onsemi.com/pdf/datasheet/esd5z2.5t1-d.pdf) | $0.65 | $6.50 |
| — | Passives | Listed in the next section | — | — | — | $4.00 | $40.00 |
| | **Parts** | | | | | **$38.76** | **$387.60** |

The ESD line is five diodes at $0.13 each ($0.65 per board; Ten is fifty diodes). The $4.00 passives line is an estimate for resistors, capacitors, inductors, the charge LED, flyback diode, BSS138, and both 2N7002s. Every other line is a listed price at quantity 10 (Heltec remains the single-unit page price). The cell is €6.49 including VAT, taken as $7.25 at 1.117 dollars per euro.

Bare boards are extra. The layout to order is `hardware/esp32/peeple.kicad_pcb`: 2-layer, about 103 × 61 mm. See [esp32-fab.md](esp32-fab.md). The cell is 60 × 40 × 6.5 mm. The older 100 × 61 mm placement is `hardware/archive/esp32-layout-v1.kicad_pcb`.

## Passives and discretes

These are inside the $4.00 estimate. Refs and values match [PCB connections](esp32-connections.md). One of each ref per board.

| Qty | Ref | Value | Role |
|---:|---|---|---|
| 2 | R3, R4 | 5.1 kΩ | USB-C CC1 and CC2 to ground |
| 1 | R25 | 150 kΩ | VBUS detect bottom (with R26 → ~3.0 V on IO41 at 5 V) |
| 1 | R26 | 100 kΩ | VBUS detect top |
| 1 | C32 | 100 nF | Across 5v_Detect |
| 1 | R1 | 10 kΩ | ESP32 EN pull-up to +3V3 |
| 1 | R2 | 470 Ω | Boot button series into EN |
| 1 | R13 | 10 kΩ | ESP32 IO3 pull-down |
| 2 | C1, C2 | 100 nF, 1 µF | EN node |
| 1 | C3 | 10 µF | ESP32 module bulk |
| 2 | C4, C5 | 10 µF, 100 nF | VBUS local |
| 1 | R7 | 2 kΩ | Charger PROG → 500 mA |
| 1 | R8 | 470 Ω | STAT to charge LED |
| 1 | D3 | LED, 0603 | Charge indicator on VBUS / STAT |
| 2 | C9, C10 | 4.7 µF | Charger VBUS and BAT |
| 1 | C6 | 4.7 µF | BAT bulk at connector |
| 2 | R16, R17 | 100 kΩ | Cell divider onto IO4 |
| 1 | C27 | 100 nF | Across BAT_ADC |
| 1 | L1 | 2.2 µH | Battery buck inductor |
| 2 | C7, C8 | 10 µF | Battery buck / U10 input |
| 2 | R5, R6 | 450 kΩ, 100 kΩ | Battery buck FB (after U10) |
| 1 | C30 | 10 µF | +3V3 at battery OR |
| 1 | R22 | 100 kΩ | U3 EN pull-up to BAT |
| 1 | Q7 | 2N7002 | Pulls U3 EN down on VBUS |
| 2 | R23, R24 | 1 kΩ, 100 kΩ | Q7 gate from VBUS |
| 1 | L3 | 2.2 µH | USB buck inductor |
| 2 | C16, C17 | 10 µF | USB buck / D9 anode |
| 1 | C29 | 4.7 µF | U9 VBUS input |
| 2 | R20, R21 | 450 kΩ, 100 kΩ | USB buck FB (after D9) |
| 1 | C31 | 10 µF | +3V3 at USB OR |
| 1 | L2 | 0 Ω | Antenna link |
| 2 | C14, C15 | 10 µF, 100 nF | Radio supply |
| 1 | R9 | 100 kΩ | GPS_PWR_N pull-up |
| 2 | R18, R19 | 1 kΩ, 5.6 kΩ | GPS UART level into RXD |
| 1 | Q6 | 2N7002 | GPS RESET_N open-drain |
| 1 | R27 | 10 kΩ | GPS reset gate pulldown |
| 3 | C18–C20 | 100 nF, 4.7 µF, 33 pF | GPS_VCC |
| 3 | C21–C23 | 100 nF, 4.7 µF, 33 pF | +3V3 near GPS / V_BCKP |
| 2 | R14, R15 | 4.7 kΩ | I2C pull-ups |
| 1 | C11 | 220 nF | LSM303AGR C1 |
| 2 | C12, C13 | 10 µF, 100 nF | Compass supplies |
| 1 | Q1 | BSS138 | Backlight low side |
| 1 | R10 | 100 kΩ | Backlight gate pulldown |
| 2 | C24, C25 | 100 nF | Screen supply |
| 2 | C26, C28 | 100 nF | BACK and ENC_SW |
| 1 | R11 | 100 kΩ | Motor gate pulldown |
| 1 | D1 | 1N4148, SOD-123 | Motor flyback |
| 1 | R12 | 100 Ω | Piezo series |

BT1 on the schematic is the pouch cell (hooks up at J2) and has no board footprint.

Footprints used on this board for these classes: resistors and small capacitors 0603; 4.7 µF and 10 µF capacitors 0805; MOSFETs SOT-23; Schottky and 1N4148 SOD-123; ESD SOD-523; USB ESD SOT-666; bucks SOT-23-5; LM66100 SC-70-6; battery header JST PH horizontal; tactiles TL3305 footprint.

## What each module requires

**ESP32-S3-WROOM-1-N8.** 25.5 × 18 mm, PCB antenna, 8 MB flash. Pin map is in [PCB connections](esp32-connections.md). Leave IO0, IO45, and IO46 open. IO3 is pulled down with R13. USB is GPIO19 / GPIO20 through U8. EN is pulled up with R1; SW3 boots the ROM. IO41 reads USB present via `5v_Detect`. The PCB image is a later firmware, separate from the Feather image and the emulator.

**HT-RA62.** 16 × 17 × 0.8 mm. SPI, busy, and reset. DIO2 inside the module steers the antenna switch. TX stays at +14 dBm for a page and −9 dBm for whisper. The spring solders through L2 to the ANT pad.

**T1-915M.** 17.5 × 5.5 mm through-hole spring. Place it away from the GPS patch and the compass. The motor is a permanent magnet.

**LC86LICMD.** 16 × 16 × 6.95 mm. UART NMEA at 9600. A low on IO39 (`GPS_PWR_N`) turns Q2 on and powers `GPS_VCC`. V_BCKP stays on `+3V3`. RXD is divided by R18 / R19 from `GPS_UART_TX` (IO38). TXD returns on `GPS_UART_RX` (IO18). RESET_N is Q6 on IO40. The breadboard drives enable high, so PCB firmware inverts that pin.

**LSM303AGRTR.** LGA-12, 2 × 2 mm. Addresses 0x19 and 0x1E. Chip-selects tied to 3.3 V for I2C. Breadboard compass offsets do not transfer.

**ERC12864FSF-9.** FPC glass via J3 (Molex 200528-0140). ST7567, 128 × 64. Bias / ratio / volume may need a tweak on this panel. Backlight on is a high on IO35 into Q1.

**654060.** JST PH 2.0. Standard charge is 500 mA with R7 = 2 kΩ on PROG. Two 100 kΩ resistors put half the cell on IO4.

**PEC11R-4220F-S0024.** 24 PPR, detents, built-in switch. Firmware needs quadrature plus a press.

**TLV62568 (×2).** SOT-23-5, 1 A, 1.5 MHz. U3 from BAT, U9 from VBUS; both FB after the OR. Q7 disables U3 while USB is present.

**LM66100.** SC-70-6 ideal diode on the battery path. CE tied to VOUT.

**MBR0530T1G.** 30 V / 500 mA Schottky OR on the USB buck output.

**AO3400A.** Logic-level N-FET for the motor (replaces a weak 2N7002 at 3.3 V gate drive).

**AO3401A.** P-FET high side for GPS power.

**GSC1030RB-3V4000.** 10 × 10 mm piezo on IO17 through 100 Ω.

**LCM1027A2445F.** 10 mm coin motor, ~80 mA. High on IO16 runs it.

## If a line will not ship

| Swap | Instead | Each | What changes |
|---|---|---:|---|
| Cell | EEMB LP103454-PCM, Amazon 10-pack $82.99 | $8.30 | 56 × 34.5 × 10.3 mm, JST PH, protection. About $1.05 more, and thicker |
| GPS | ATGM336H-5NR32-G (LCSC C49233943, $1.68) plus Bat Wireless BWGPSCNX8-8B1 (LCSC C496567, $1.59) | $3.27 | Saves about $4.26. Needs a board IPEX jack and a 47 nH from the module RF bias pin |
| Screen | EastRising ERC12864FSF-11, 0.96 inch | $1.28 | Same ST7567 SPI. Outline 27.30 × 21.50 × 3.3 mm. Only if the pager gets smaller |
| USB ESD | USBLC6-2SC6 (LCSC C7519) | ~$0.16 | Same function, SOT-23-6 instead of SOT-666 |

## Related

- [PCB connections](esp32-connections.md) — nets and pins this list follows
- [Hardware](feather.md) — why each function exists, and the Feather pin contract
- [Prototype assembly](breadboard-assembly.md) — the breadboard this board replaces
