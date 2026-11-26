# RP2350 connections

Connections on `hardware/rp2350/peeple.kicad_sch`, the v3 board. That schematic is the source of truth; this document follows it. Every net on the sheet is joined by a label on the pin, so the tables below are the whole drawing. The last column is the other end of the net, or the rail name for `GND`, `+3V3`, `+1V1`, `VBUS`, `VSYS`, `BAT`, and `GPS_VCC`.

`BAT` is the cell. `VBUS` is USB 5 V. `VSYS` is whichever of the two is feeding the buck. One TLV62568 makes `+3V3` for everything. `+1V1` is the RP2354A core rail from its own regulator. `GPS_VCC` is the switched GPS rail.

## USB-C

J1 is the HRO TYPE-C-31-M-12. Both sides of the receptacle are tied together. CC1 and CC2 each have 5.1 kΩ to ground, so a USB-C charger offers 5 V. D+ and D− pass through U8 (pins 1/6 and 3/4 are the same line inside the part) and then through 27 Ω resistors R19 / R20 that sit next to U1. R23 / R22 put about 3.0 V on `VBUS_DET` when USB is present.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| J1 | TYPE-C-31-M-12 | A1, A12, B1, B12 GND; SH SHIELD | GND | GND |
| J1 | TYPE-C-31-M-12 | A4, A9, B4, B9 VBUS | VBUS | VBUS |
| J1 | TYPE-C-31-M-12 | A5 CC1 | CC1 | R3 |
| J1 | TYPE-C-31-M-12 | A6, B6 D+ | USB_C_DP | R19; U8 I/O1 |
| J1 | TYPE-C-31-M-12 | A7, B7 D- | USB_C_DM | R20; U8 I/O2 |
| J1 | TYPE-C-31-M-12 | A8 SBU1; B8 SBU2 |  | Leave open |
| J1 | TYPE-C-31-M-12 | B5 CC2 | CC2 | R4 |
| U8 | USBLC6-2SC6 | 1, 6 I/O1 | USB_C_DP | J1 D+; R19 |
| U8 | USBLC6-2SC6 | 2 GND | GND | GND |
| U8 | USBLC6-2SC6 | 3, 4 I/O2 | USB_C_DM | J1 D-; R20 |
| U8 | USBLC6-2SC6 | 5 VBUS | VBUS | VBUS |
| R3 | 5.1 kΩ | 1 | CC1 | J1 CC1 |
| R3 | 5.1 kΩ | 2 | GND | GND |
| R4 | 5.1 kΩ | 1 | CC2 | J1 CC2 |
| R4 | 5.1 kΩ | 2 | GND | GND |
| R19 | 27 Ω | 1 | USB_C_DP | J1 D+; U8 I/O1 |
| R19 | 27 Ω | 2 | USB_DP | U1 pin 52, USB_DP |
| R20 | 27 Ω | 1 | USB_C_DM | J1 D-; U8 I/O2 |
| R20 | 27 Ω | 2 | USB_DM | U1 pin 51, USB_DM |
| C1 | 10 µF | 1 | VBUS | VBUS |
| C1 | 10 µF | 2 | GND | GND |
| C2 | 100 nF | 1 | VBUS | VBUS |
| C2 | 100 nF | 2 | GND | GND |
| R23 | 100 kΩ | 1 | VBUS | VBUS |
| R23 | 100 kΩ | 2 | VBUS_DET | U1 pin 41, GPIO27/ADC1; C9; R22 |
| R22 | 150 kΩ | 1 | VBUS_DET | U1 pin 41, GPIO27/ADC1; C9; R23 |
| R22 | 150 kΩ | 2 | GND | GND |
| C9 | 100 nF | 1 | VBUS_DET | U1 pin 41, GPIO27/ADC1; R22; R23 |
| C9 | 100 nF | 2 | GND | GND |

## Charger and cell

U2 is the TP4054 (4.2 V float). R7 = 2 kΩ on PROG sets 500 mA (I = 1000 V / R_PROG). CHRG is open drain: it lights D3 from VBUS while the cell charges and lets go when it is full. J2 is the cell; pin 1 is positive. R16 / R17 put half the cell on `BAT_ADC` (GPIO26, ADC0).

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| U2 | TP4054 | 1 CHRG | CHG_STAT | R8 |
| U2 | TP4054 | 2 GND | GND | GND |
| U2 | TP4054 | 3 BAT | BAT | BAT |
| U2 | TP4054 | 4 VCC | VBUS | VBUS |
| U2 | TP4054 | 5 PROG | CHG_PROG | R7 |
| R7 | 2 kΩ | 1 | CHG_PROG | U2 PROG |
| R7 | 2 kΩ | 2 | GND | GND |
| R8 | 1 kΩ | 1 | CHG_LED | D3 |
| R8 | 1 kΩ | 2 | CHG_STAT | U2 CHRG |
| D3 | NCD0805R1 | 1 K | CHG_LED | R8 |
| D3 | NCD0805R1 | 2 A | VBUS | VBUS |
| C3 | 4.7 µF | 1 | VBUS | VBUS |
| C3 | 4.7 µF | 2 | GND | GND |
| C4 | 4.7 µF | 1 | BAT | BAT |
| C4 | 4.7 µF | 2 | GND | GND |
| J2 | PH-2PWB | 1 | BAT | BAT |
| J2 | PH-2PWB | 2 | GND | GND |
| R16 | 100 kΩ | 1 | BAT | BAT |
| R16 | 100 kΩ | 2 | BAT_ADC | U1 pin 40, GPIO26/ADC0; C8; R17 |
| R17 | 100 kΩ | 1 | BAT_ADC | U1 pin 40, GPIO26/ADC0; C8; R16 |
| R17 | 100 kΩ | 2 | GND | GND |
| C8 | 100 nF | 1 | BAT_ADC | U1 pin 40, GPIO26/ADC0; R16; R17 |
| C8 | 100 nF | 2 | GND | GND |

## Power path and buck

`VSYS` feeds the one buck. USB reaches `VSYS` through Schottky D2. The cell reaches `VSYS` through P-FET Q3, whose gate is VBUS: with no USB, R21 holds the gate at ground and Q3 is on; with USB, the gate is at 5 V and Q3 is off, so the charger has the cell to itself while the load runs from USB. U3 bucks `VSYS` to `+3V3`. R5 / R6 (453 kΩ / 100 kΩ on a 0.6 V reference) set 3.32 V. Below about 3.4 V on the cell the buck runs at 100 % duty and `+3V3` follows the cell down.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| D2 | B5819W | 1 K | VSYS | VSYS |
| D2 | B5819W | 2 A | VBUS | VBUS |
| Q3 | AO3401A | 1 G | VBUS | VBUS |
| Q3 | AO3401A | 2 S | VSYS | VSYS |
| Q3 | AO3401A | 3 D | BAT | BAT |
| R21 | 100 kΩ | 1 | VBUS | VBUS |
| R21 | 100 kΩ | 2 | GND | GND |
| U3 | TLV62568DBVR | 1 EN; 4 VIN | VSYS | VSYS |
| U3 | TLV62568DBVR | 2 GND | GND | GND |
| U3 | TLV62568DBVR | 3 SW | BUCK_SW | L1 |
| U3 | TLV62568DBVR | 5 FB | BUCK_FB | R5; R6 |
| L1 | 2.2 µH | 1 | BUCK_SW | U3 SW |
| L1 | 2.2 µH | 2 | +3V3 | +3V3 |
| R5 | 453 kΩ | 1 | +3V3 | +3V3 |
| R5 | 453 kΩ | 2 | BUCK_FB | R6; U3 FB |
| R6 | 100 kΩ | 1 | BUCK_FB | R5; U3 FB |
| R6 | 100 kΩ | 2 | GND | GND |
| C5 | 10 µF | 1 | VSYS | VSYS |
| C5 | 10 µF | 2 | GND | GND |
| C6 | 10 µF | 1 | +3V3 | +3V3 |
| C6 | 10 µF | 2 | GND | GND |
| C7 | 10 µF | 1 | +3V3 | +3V3 |
| C7 | 10 µF | 2 | GND | GND |

## RP2354A

U1 is the RP2354A: the RP2350A die with 2 MB of flash in the same QFN-60. The QSPI data and clock pins stay open because the flash is inside. The core supply comes from the chip's own switching regulator: VREG_LX drives L3 (3.3 µH, polarity dot on the VREG_LX pad) into `+1V1`, which feeds DVDD and VREG_FB. R1 / C23 filter VREG_AVDD from `+3V3`. Y1 is the 12 MHz crystal; R13 is in series with XOUT. SW3 pulls QSPI_SS low through R2 to start the USB bootloader. SW4 pulls RUN low; RUN has an internal pull-up of about 50 kΩ. J4 is SWD (SWCLK, GND, SWDIO, the Raspberry Pi Debug Probe order).

Decoupling placement, which the schematic cannot show: C10–C15 one at each IOVDD pin (1, 11, 20, 30, 38, 45). C16 between pins 53 and 54. C17 at ADC_AVDD (44). C18–C20 one at each DVDD pin (6, 23, 39). C21 at VREG_VIN (49). C22 at the `+1V1` end of L3. Copy the regulator layout from the Raspberry Pi minimal design; the RP2350 datasheet says other layouts are at your own risk.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| U1 | RP2354A | 1, 11, 20, 30, 38, 45 IOVDD; 44 ADC_AVDD; 49 VREG_VIN; 53 USB_OTP_VDD; 54 QSPI_IOVDD | +3V3 | +3V3 |
| U1 | RP2354A | 6, 23, 39 DVDD; 50 VREG_FB | +1V1 | +1V1 |
| U1 | RP2354A | 21 XIN | XIN | C24; Y1 pin 1 |
| U1 | RP2354A | 22 XOUT | XOUT | R13 |
| U1 | RP2354A | 24 SWCLK | SWCLK | J4 pin 1 |
| U1 | RP2354A | 25 SWDIO | SWDIO | J4 pin 3 |
| U1 | RP2354A | 26 RUN | RUN | SW4 |
| U1 | RP2354A | 46 VREG_AVDD | VREG_AVDD | C23; R1 |
| U1 | RP2354A | 47 VREG_PGND; 61 GND | GND | GND |
| U1 | RP2354A | 48 VREG_LX | VREG_LX | L3 |
| U1 | RP2354A | 51 USB_DM | USB_DM | R20 |
| U1 | RP2354A | 52 USB_DP | USB_DP | R19 |
| U1 | RP2354A | 60 QSPI_SS | QSPI_SS | R2 |
| L3 | 3.3 µH | 1 | VREG_LX | U1 pin 48, VREG_LX |
| L3 | 3.3 µH | 2 | +1V1 | +1V1 |
| R1 | 33 Ω | 1 | +3V3 | +3V3 |
| R1 | 33 Ω | 2 | VREG_AVDD | U1 pin 46, VREG_AVDD; C23 |
| C23 | 4.7 µF | 1 | VREG_AVDD | U1 pin 46, VREG_AVDD; R1 |
| C23 | 4.7 µF | 2 | GND | GND |
| C21 | 4.7 µF | 1 | +3V3 | +3V3 |
| C21 | 4.7 µF | 2 | GND | GND |
| C22 | 4.7 µF | 1 | +1V1 | +1V1 |
| C22 | 4.7 µF | 2 | GND | GND |
| C10 | 100 nF | 1 | +3V3 | +3V3 |
| C10 | 100 nF | 2 | GND | GND |
| C11 | 100 nF | 1 | +3V3 | +3V3 |
| C11 | 100 nF | 2 | GND | GND |
| C12 | 100 nF | 1 | +3V3 | +3V3 |
| C12 | 100 nF | 2 | GND | GND |
| C13 | 100 nF | 1 | +3V3 | +3V3 |
| C13 | 100 nF | 2 | GND | GND |
| C14 | 100 nF | 1 | +3V3 | +3V3 |
| C14 | 100 nF | 2 | GND | GND |
| C15 | 100 nF | 1 | +3V3 | +3V3 |
| C15 | 100 nF | 2 | GND | GND |
| C16 | 100 nF | 1 | +3V3 | +3V3 |
| C16 | 100 nF | 2 | GND | GND |
| C17 | 100 nF | 1 | +3V3 | +3V3 |
| C17 | 100 nF | 2 | GND | GND |
| C18 | 100 nF | 1 | +1V1 | +1V1 |
| C18 | 100 nF | 2 | GND | GND |
| C19 | 100 nF | 1 | +1V1 | +1V1 |
| C19 | 100 nF | 2 | GND | GND |
| C20 | 100 nF | 1 | +1V1 | +1V1 |
| C20 | 100 nF | 2 | GND | GND |
| Y1 | ABM8-272-T3 | 1 | XIN | U1 pin 21, XIN; C24 |
| Y1 | ABM8-272-T3 | 2, 4 G | GND | GND |
| Y1 | ABM8-272-T3 | 3 | XOUT_X | C25; R13 |
| R13 | 1 kΩ | 1 | XOUT | U1 pin 22, XOUT |
| R13 | 1 kΩ | 2 | XOUT_X | C25; Y1 pin 3 |
| C24 | 15 pF | 1 | XIN | U1 pin 21, XIN; Y1 pin 1 |
| C24 | 15 pF | 2 | GND | GND |
| C25 | 15 pF | 1 | XOUT_X | R13; Y1 pin 3 |
| C25 | 15 pF | 2 | GND | GND |
| R2 | 1 kΩ | 1 | QSPI_SS | U1 pin 60, QSPI_SS |
| R2 | 1 kΩ | 2 | BOOTSEL | SW3 |
| SW3 | TS-1187A-B-A-B | 1 | BOOTSEL | R2 |
| SW3 | TS-1187A-B-A-B | 2 | GND | GND |
| SW4 | TS-1187A-B-A-B | 1 | RUN | U1 pin 26, RUN |
| SW4 | TS-1187A-B-A-B | 2 | GND | GND |
| J4 | 2.54 mm 1x3 header (optional) | 1 | SWCLK | U1 pin 24, SWCLK |
| J4 | 2.54 mm 1x3 header (optional) | 2 | GND | GND |
| J4 | 2.54 mm 1x3 header (optional) | 3 | SWDIO | U1 pin 25, SWDIO |

GPIO map:

| GPIO | Pin | Net | Other end |
|---|---|---|---|
| 0 | 2 | GPS_UART_TX | R18 |
| 1 | 3 | GPS_UART_RX | U5 TXD0 |
| 2 | 4 | GPS_PWR_N | Q2 G; R9 |
| 3 | 5 | GPS_RST_N | U5 RESET |
| 4 | 7 | I2C_SDA | R15; U6 SDx; U7 SDA |
| 5 | 8 | I2C_SCL | R14; U6 SCx; U7 SCK |
| 6 | 9 | ENC_A | SW1 pin A; U9 IO1 |
| 7 | 10 | ENC_B | SW1 pin B; U9 IO2 |
| 8 | 12 | ENC_SW | C38; SW1 pin S1; U9 IO3 |
| 9 | 13 | BACK | C37; SW2; U9 IO4 |
| 10 | 14 | MOTOR | Q4 G; R11 |
| 11 | 15 | PIEZO | R12 |
| 12 | 16 | SPI_MISO | U4 MISO |
| 13 | 17 | RF_CS | U4 NSS |
| 14 | 18 | SPI_SCK | J3 pin 6; U4 SCK |
| 15 | 19 | SPI_MOSI | J3 pin 7; U4 MOSI |
| 16 | 27 | RF_RST | U4 RST |
| 17 | 28 | RF_BUSY | U4 BUSY |
| 18 | 29 | RF_DIO1 | U4 DIO1 |
| 19 | 31 | LCD_CS | J3 pin 3 |
| 20 | 32 | LCD_A0 | J3 pin 5 |
| 21 | 33 | LCD_RST | J3 pin 4 |
| 22 | 34 | LCD_BL | Q1 G; R10 |
| 23 | 35 |  | Spare, leave open |
| 24 | 36 |  | Spare, leave open |
| 25 | 37 |  | Spare, leave open |
| 26 (ADC0) | 40 | BAT_ADC | C8; R16; R17 |
| 27 (ADC1) | 41 | VBUS_DET | C9; R22; R23 |
| 28 (ADC2) | 42 |  | Spare, leave open |
| 29 (ADC3) | 43 |  | Spare, leave open |

UART0 is GPIO0 / GPIO1. I2C0 is GPIO4 / GPIO5. SPI1 is GPIO12 (RX), GPIO14 (SCK), GPIO15 (TX); GPIO13 and GPIO19 are chip selects driven as plain outputs. Spare pins: GPIO23–25, GPIO28, GPIO29.

## LoRa radio

U4 is the HT-RA62, unchanged from v2. AE1 is the spring antenna. L2 is a 0 Ω link on the antenna path. DIO1 now reaches GPIO18 so firmware can take an interrupt instead of polling BUSY; it is optional. DIO2, DIO3, TXEN, and RXEN stay open.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| U4 | HT-RA62 (915 MHz) | 1 ANT | RF_ANT | L2 |
| U4 | HT-RA62 (915 MHz) | 2, 9, 16 GND | GND | GND |
| U4 | HT-RA62 (915 MHz) | 3 3V3 | +3V3 | +3V3 |
| U4 | HT-RA62 (915 MHz) | 4 RST | RF_RST | U1 pin 27, GPIO16 |
| U4 | HT-RA62 (915 MHz) | 5 TXEN; 7 DIO2; 8 DIO3; 11 RXEN |  | Leave open |
| U4 | HT-RA62 (915 MHz) | 6 DIO1 | RF_DIO1 | U1 pin 29, GPIO18 |
| U4 | HT-RA62 (915 MHz) | 10 BUSY | RF_BUSY | U1 pin 28, GPIO17 |
| U4 | HT-RA62 (915 MHz) | 12 SCK | SPI_SCK | U1 pin 18, GPIO14; J3 pin 6 |
| U4 | HT-RA62 (915 MHz) | 13 MISO | SPI_MISO | U1 pin 16, GPIO12 |
| U4 | HT-RA62 (915 MHz) | 14 MOSI | SPI_MOSI | U1 pin 19, GPIO15; J3 pin 7 |
| U4 | HT-RA62 (915 MHz) | 15 NSS | RF_CS | U1 pin 17, GPIO13 |
| L2 | 0 Ω | 1 | RF_ANT | U4 ANT |
| L2 | 0 Ω | 2 | ANT | AE1 |
| AE1 | T1-915M | 1 A | ANT | L2 |
| C26 | 10 µF | 1 | +3V3 | +3V3 |
| C26 | 10 µF | 2 | GND | GND |
| C27 | 100 nF | 1 | +3V3 | +3V3 |
| C27 | 100 nF | 2 | GND | GND |

## GPS

U5 is the ATGM336H-5NR32-G. Q2 switches `GPS_VCC`; a low on GPIO2 (`GPS_PWR_N`) turns it on. ON/OFF is tied to `GPS_VCC`, so the module runs whenever it has power. VBAT stays on `+3V3` so the receiver keeps time and almanac between fixes. The module's I/O runs at its VCC, which is the same 3.3 V as U1, so RXD0 needs no divider; R18 limits current if U1 drives TX while the GPS is off. RESET goes straight to GPIO3. Drive it low to reset and leave the pin as an input otherwise; never drive it high. The antenna is an active patch on an IPEX cable at J5. VCC_RF biases it through L4.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| U5 | ATGM336H-5NR32-G | 1, 10, 12 GND | GND | GND |
| U5 | ATGM336H-5NR32-G | 2 TXD0 | GPS_UART_RX | U1 pin 3, GPIO1 |
| U5 | ATGM336H-5NR32-G | 3 RXD0 | GPS_RXD | R18 |
| U5 | ATGM336H-5NR32-G | 4 1PPS; 7, 13, 15, 18 RSVD; 16 RXD1; 17 TXD1 |  | Leave open |
| U5 | ATGM336H-5NR32-G | 5 ON/OFF; 8 VCC | GPS_VCC | GPS_VCC |
| U5 | ATGM336H-5NR32-G | 6 VBAT | +3V3 | +3V3 |
| U5 | ATGM336H-5NR32-G | 9 RESET | GPS_RST_N | U1 pin 5, GPIO3 |
| U5 | ATGM336H-5NR32-G | 11 RF_IN | GPS_RF | J5; L4 |
| U5 | ATGM336H-5NR32-G | 14 VCC_RF | GPS_VCC_RF | L4 |
| Q2 | AO3401A | 1 G | GPS_PWR_N | U1 pin 4, GPIO2; R9 |
| Q2 | AO3401A | 2 S | +3V3 | +3V3 |
| Q2 | AO3401A | 3 D | GPS_VCC | GPS_VCC |
| R9 | 100 kΩ | 1 | +3V3 | +3V3 |
| R9 | 100 kΩ | 2 | GPS_PWR_N | U1 pin 4, GPIO2; Q2 G |
| R18 | 1 kΩ | 1 | GPS_UART_TX | U1 pin 2, GPIO0 |
| R18 | 1 kΩ | 2 | GPS_RXD | U5 RXD0 |
| L4 | 47 nH | 1 | GPS_VCC_RF | U5 VCC_RF |
| L4 | 47 nH | 2 | GPS_RF | J5; U5 RF_IN |
| J5 | U.FL-R-SMT-1(80) | 1 In | GPS_RF | L4; U5 RF_IN |
| J5 | U.FL-R-SMT-1(80) | 2 Ext | GND | GND |
| C28 | 10 µF | 1 | GPS_VCC | GPS_VCC |
| C28 | 10 µF | 2 | GND | GND |
| C29 | 100 nF | 1 | GPS_VCC | GPS_VCC |
| C29 | 100 nF | 2 | GND | GND |
| C30 | 100 nF | 1 | +3V3 | +3V3 |
| C30 | 100 nF | 2 | GND | GND |

## Motion sensors

U6 is the SC7A20H accelerometer at I2C address 0x19: SDO (pin 1) is tied high, CS (pin 10) is tied high for I2C, and pin 4 goes to ground as the datasheet asks. U7 is the QMC5883P magnetometer at 0x2C. C34 is its 4.7 µF reservoir on C1. R14 / R15 are the bus pull-ups. Interrupts stay open. Keep both sensors away from the motor, the piezo, and L1.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| U6 | SC7A20HTR | 1 SDO; 3 VDDIO; 7 VDD; 10 CS | +3V3 | +3V3 |
| U6 | SC7A20HTR | 2 SDx | I2C_SDA | U1 pin 7, GPIO4; R15; U7 SDA |
| U6 | SC7A20HTR | 4 NC; 8 GNDIO; 9 GND | GND | GND |
| U6 | SC7A20HTR | 5 INT1; 6 INT2; 11 NC |  | Leave open |
| U6 | SC7A20HTR | 12 SCx | I2C_SCL | U1 pin 8, GPIO5; R14; U7 SCK |
| U7 | QMC5883P | 1 SCK | I2C_SCL | U1 pin 8, GPIO5; R14; U6 SCx |
| U7 | QMC5883P | 2 VDD | +3V3 | +3V3 |
| U7 | QMC5883P | 3, 4, 5, 6, 7, 8, 12, 13, 14, 15 NC |  | Leave open |
| U7 | QMC5883P | 9, 11 GND | GND | GND |
| U7 | QMC5883P | 10 C1 | MAG_C1 | C34 |
| U7 | QMC5883P | 16 SDA | I2C_SDA | U1 pin 7, GPIO4; R15; U6 SDx |
| R14 | 4.7 kΩ | 1 | +3V3 | +3V3 |
| R14 | 4.7 kΩ | 2 | I2C_SCL | U1 pin 8, GPIO5; U6 SCx; U7 SCK |
| R15 | 4.7 kΩ | 1 | +3V3 | +3V3 |
| R15 | 4.7 kΩ | 2 | I2C_SDA | U1 pin 7, GPIO4; U6 SDx; U7 SDA |
| C31 | 100 nF | 1 | +3V3 | +3V3 |
| C31 | 100 nF | 2 | GND | GND |
| C32 | 100 nF | 1 | +3V3 | +3V3 |
| C32 | 100 nF | 2 | GND | GND |
| C33 | 100 nF | 1 | +3V3 | +3V3 |
| C33 | 100 nF | 2 | GND | GND |
| C34 | 4.7 µF | 1 | MAG_C1 | U7 C1 |
| C34 | 4.7 µF | 2 | GND | GND |

## Screen

J3 is the Molex 200528-0140 socket for the ERC12864FSF-9 glass. SPI is shared with the radio. BLA is on `+3V3`. BLK is switched low by Q1; a high on GPIO22 turns the lamp on. R10 holds the gate low through reset.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| J3 | Molex 200528-0140 | 1, 8 | +3V3 | +3V3 |
| J3 | Molex 200528-0140 | 2 | LCD_BLK | Q1 D |
| J3 | Molex 200528-0140 | 3 | LCD_CS | U1 pin 31, GPIO19 |
| J3 | Molex 200528-0140 | 4 | LCD_RST | U1 pin 33, GPIO21 |
| J3 | Molex 200528-0140 | 5 | LCD_A0 | U1 pin 32, GPIO20 |
| J3 | Molex 200528-0140 | 6 | SPI_SCK | U1 pin 18, GPIO14; U4 SCK |
| J3 | Molex 200528-0140 | 7 | SPI_MOSI | U1 pin 19, GPIO15; U4 MOSI |
| J3 | Molex 200528-0140 | 9 | GND | GND |
| J3 | Molex 200528-0140 | 10, 11, 12, 13, 14 |  | Leave open |
| Q1 | BSS138 | 1 G | LCD_BL | U1 pin 34, GPIO22; R10 |
| Q1 | BSS138 | 2 S | GND | GND |
| Q1 | BSS138 | 3 D | LCD_BLK | J3 pin 2 |
| R10 | 10 kΩ | 1 | LCD_BL | U1 pin 34, GPIO22; Q1 G |
| R10 | 10 kΩ | 2 | GND | GND |
| C35 | 100 nF | 1 | +3V3 | +3V3 |
| C35 | 100 nF | 2 | GND | GND |
| C36 | 100 nF | 1 | +3V3 | +3V3 |
| C36 | 100 nF | 2 | GND | GND |

## Controls

SW1 is the PEC11R encoder with its push switch. SW2 is Back. Commons go to ground and firmware enables the internal pull-ups. U9 clamps all four lines, with its VP pin on `+3V3`. C37 / C38 are local bypasses on BACK and ENC_SW.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| SW1 | PEC11R-4220F-S0024 | A | ENC_A | U1 pin 9, GPIO6; U9 IO1 |
| SW1 | PEC11R-4220F-S0024 | B | ENC_B | U1 pin 10, GPIO7; U9 IO2 |
| SW1 | PEC11R-4220F-S0024 | C, S2 | GND | GND |
| SW1 | PEC11R-4220F-S0024 | S1 | ENC_SW | U1 pin 12, GPIO8; C38; U9 IO3 |
| SW2 | TS-1187A-B-A-B | 1 | BACK | U1 pin 13, GPIO9; C37; U9 IO4 |
| SW2 | TS-1187A-B-A-B | 2 | GND | GND |
| U9 | SRV05-4 | 1 IO1 | ENC_A | U1 pin 9, GPIO6; SW1 pin A |
| U9 | SRV05-4 | 2 VN | GND | GND |
| U9 | SRV05-4 | 3 IO2 | ENC_B | U1 pin 10, GPIO7; SW1 pin B |
| U9 | SRV05-4 | 4 IO3 | ENC_SW | U1 pin 12, GPIO8; C38; SW1 pin S1 |
| U9 | SRV05-4 | 5 VP | +3V3 | +3V3 |
| U9 | SRV05-4 | 6 IO4 | BACK | U1 pin 13, GPIO9; C37; SW2 |
| C37 | 100 nF | 1 | BACK | U1 pin 13, GPIO9; SW2; U9 IO4 |
| C37 | 100 nF | 2 | GND | GND |
| C38 | 100 nF | 1 | ENC_SW | U1 pin 12, GPIO8; SW1 pin S1; U9 IO3 |
| C38 | 100 nF | 2 | GND | GND |

## Motor and piezo

M1 is the coin motor with Q4 as the low-side switch. A high on GPIO10 runs it; R11 holds it off through reset. D1 clamps the motor's flyback. BZ1 is driven from GPIO11 (PWM slice 5, channel B) through R12.

| Ref | Part | Pin | Net | Other end |
|---|---|---|---|---|
| M1 | JYC1027 | 1 + | +3V3 | +3V3 |
| M1 | JYC1027 | 2 - | MOTOR_N | D1; Q4 D |
| Q4 | AO3400A | 1 G | MOTOR | U1 pin 14, GPIO10; R11 |
| Q4 | AO3400A | 2 S | GND | GND |
| Q4 | AO3400A | 3 D | MOTOR_N | D1; M1 |
| R11 | 10 kΩ | 1 | MOTOR | U1 pin 14, GPIO10; Q4 G |
| R11 | 10 kΩ | 2 | GND | GND |
| D1 | 1N4148W | 1 K | +3V3 | +3V3 |
| D1 | 1N4148W | 2 A | MOTOR_N | M1; Q4 D |
| BZ1 | GSC1030RB-3V4000 | 1 + | PIEZO_P | R12 |
| BZ1 | GSC1030RB-3V4000 | 2 - | GND | GND |
| R12 | 100 Ω | 1 | PIEZO | U1 pin 15, GPIO11 |
| R12 | 100 Ω | 2 | PIEZO_P | BZ1 |
