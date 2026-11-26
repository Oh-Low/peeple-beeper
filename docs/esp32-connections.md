# ESP32 connections

> v2. The RP2350 board replaces it: [rp2350-connections.md](rp2350-connections.md).

Connections on `hardware/esp32/peeple.kicad_sch`. That schematic is the source of truth; this document follows it. Each section is one circuit. The callout column is a pin that reaches the ESP32, `+3V3`, `BAT`, `VBUS`, or ground. A blank callout stays inside the circuit.

`BAT` is the cell. `VBUS` is USB 5 V. Two TLV62568 bucks feed `+3V3`: U3 from the cell through ideal diode U10, and U9 from VBUS through Schottky D9. `GPS_VCC` is the switched rail for the GPS module. The ESP32 runs from `+3V3`.

## USB-C

J1 is the GCT USB4105-GF-A. Both sides of the receptacle are tied together. VBUS feeds the charger and the USB-side buck. D+ and D− go through U8 to the ESP32 USB pins. SW3 is the boot button on EN.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| J1 | USB4105-GF-A | A1, A12, B1, B12 GND | GND | GND |
| J1 | USB4105-GF-A | SHIELD | GND | GND |
| J1 | USB4105-GF-A | A4, A9, B4, B9 VBUS | VBUS | Charger, U9, detect |
| J1 | USB4105-GF-A | A6, B6 D+ | | U8 I/O1 |
| J1 | USB4105-GF-A | A7, B7 D− | | U8 I/O2 |
| J1 | USB4105-GF-A | A5 CC1 | CC1 | R3 to GND |
| J1 | USB4105-GF-A | B5 CC2 | CC2 | R4 to GND |
| J1 | USB4105-GF-A | A8 SBU1, B8 SBU2 | | Leave open |
| R3 | 5.1 kΩ | 1 | GND | GND |
| R3 | 5.1 kΩ | 2 | CC1 | |
| R4 | 5.1 kΩ | 1 | GND | GND |
| R4 | 5.1 kΩ | 2 | CC2 | |
| U8 | USBLC6-2P6 | 5 VBUS | VBUS | USB 5 V |
| U8 | USBLC6-2P6 | 2 GND | GND | GND |
| U8 | USBLC6-2P6 | 3 I/O1 | | J1 D+ |
| U8 | USBLC6-2P6 | 1 I/O2 | | J1 D− |
| U8 | USBLC6-2P6 | 4 I/O1 | USB_D+ | ESP32 U1 pin 14, GPIO20 |
| U8 | USBLC6-2P6 | 6 I/O2 | USB_D− | ESP32 U1 pin 13, GPIO19 |
| C4 | 10 µF | 1 | GND | GND |
| C4 | 10 µF | 2 | VBUS | |
| C5 | 100 nF | 1 | GND | GND |
| C5 | 100 nF | 2 | VBUS | |
| R25 | 150 kΩ | 1 | GND | GND |
| R25 | 150 kΩ | 2 | 5v_Detect | ESP32 U1 pin 34, IO41 |
| R26 | 100 kΩ | 1 | 5v_Detect | |
| R26 | 100 kΩ | 2 | VBUS | |
| C32 | 100 nF | 1 | GND | GND |
| C32 | 100 nF | 2 | 5v_Detect | |
| SW3 | Boot | 1 | GND | GND |
| SW3 | Boot | 2 | | R2 to EN node |
| R2 | 470 Ω | 1 | | SW3, EN node |
| R2 | 470 Ω | 2 | | With R1 / D2 / C1 / C2 on EN |
| R1 | 10 kΩ | 1 | +3V3 | +3V3 |
| R1 | 10 kΩ | 2 | ESP_EN | ESP32 EN |
| D2 | ESD5Zxx | 1 K | ESP_EN | |
| D2 | ESD5Zxx | 2 A | GND | GND |
| C1 | 100 nF | 1 | ESP_EN | |
| C1 | 100 nF | 2 | GND | GND |
| C2 | 1 µF | 1 | ESP_EN | |
| C2 | 1 µF | 2 | GND | GND |

At 5 V on VBUS, the R26 / R25 divider puts about 3.0 V on `5v_Detect`.

## Battery connector

J2 is the JST PH battery header. Pin 1 is the cell positive. BT1 on the schematic is the pouch cell itself and is not a board footprint. The divider lets the ESP32 read the cell.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| J2 | S2B-PH-SM4-TB | 1 | BAT | Cell positive |
| J2 | S2B-PH-SM4-TB | 2 | GND | GND |
| R16 | 100 kΩ | 1 | BAT_ADC | ESP32 U1 pin 4, IO4 |
| R16 | 100 kΩ | 2 | BAT | |
| R17 | 100 kΩ | 1 | GND | GND |
| R17 | 100 kΩ | 2 | BAT_ADC | |
| C27 | 100 nF | 1 | GND | GND |
| C27 | 100 nF | 2 | BAT_ADC | |
| C6 | 4.7 µF | 1 | BAT | |
| C6 | 4.7 µF | 2 | GND | GND |

## Charger

U2 is the MCP73831T-2ACI/OT (4.20 V). PROG is 2 kΩ, which sets 500 mA. STAT drives D3 through R8.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U2 | MCP73831T-2ACI/OT | 4 VDD | VBUS | USB 5 V |
| U2 | MCP73831T-2ACI/OT | 3 VBAT | BAT | Cell, J2 pin 1 |
| U2 | MCP73831T-2ACI/OT | 2 VSS | GND | GND |
| U2 | MCP73831T-2ACI/OT | 5 PROG | CHG_PROG | R7 to GND |
| U2 | MCP73831T-2ACI/OT | 1 STAT | CHG_STAT | R8 to D3 |
| R7 | 2 kΩ | 1 | CHG_PROG | |
| R7 | 2 kΩ | 2 | GND | GND |
| C9 | 4.7 µF | 1 | VBUS | |
| C9 | 4.7 µF | 2 | GND | GND |
| C10 | 4.7 µF | 1 | BAT | |
| C10 | 4.7 µF | 2 | GND | GND |
| R8 | 470 Ω | 1 | | D3 cathode |
| R8 | 470 Ω | 2 | CHG_STAT | |
| D3 | LED | A | VBUS | |
| D3 | LED | K | | R8 |

## Dual buck and OR

U3 bucks `BAT` to a pre-OR rail into U10 (LM66100). U9 bucks `VBUS` to a pre-OR rail into D9. Both OR outputs are `+3V3`. Feedback for each buck is taken after its OR device so the FB divider sees the load rail. Each FB pair is 450 kΩ / 100 kΩ on a 0.6 V reference → about 3.30 V.

Q7 pulls U3 EN low while VBUS is present (R23 / R24), so the battery buck yields to USB. R22 pulls EN to BAT when Q7 is off.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U3 | TLV62568DBVT | 4 VIN | BAT | Cell |
| U3 | TLV62568DBVT | 1 EN | | Q7 drain / R22 to BAT |
| U3 | TLV62568DBVT | 2 GND | GND | GND |
| U3 | TLV62568DBVT | 3 SW | | L1 pin 1 |
| U3 | TLV62568DBVT | 5 FB | | R5 / R6 midpoint |
| L1 | 2.2 µH | 1 | | U3 SW |
| L1 | 2.2 µH | 2 | | U10 VIN |
| C7 | 10 µF | 1 | | U10 VIN |
| C7 | 10 µF | 2 | GND | GND |
| C8 | 10 µF | 1 | | U10 VIN |
| C8 | 10 µF | 2 | GND | GND |
| U10 | LM66100DCK | 1 VIN | | L1 / C7 / C8 |
| U10 | LM66100DCK | 3 VOUT | +3V3 | +3V3 |
| U10 | LM66100DCK | 6 CE | +3V3 | Tied to VOUT |
| U10 | LM66100DCK | 2, 5 GND | GND | GND |
| U10 | LM66100DCK | 4 NC | | Leave open |
| R5 | 450 kΩ | 1 | +3V3 | +3V3 |
| R5 | 450 kΩ | 2 | | U3 FB |
| R6 | 100 kΩ | 1 | | U3 FB |
| R6 | 100 kΩ | 2 | GND | GND |
| C30 | 10 µF | 1 | +3V3 | +3V3 |
| C30 | 10 µF | 2 | GND | GND |
| R22 | 100 kΩ | 1 | | U3 EN |
| R22 | 100 kΩ | 2 | BAT | |
| Q7 | 2N7002 | 1 G | | R23 / R24 |
| Q7 | 2N7002 | 2 S | GND | GND |
| Q7 | 2N7002 | 3 D | | U3 EN |
| R23 | 1 kΩ | 1 | | Q7 gate |
| R23 | 1 kΩ | 2 | VBUS | |
| R24 | 100 kΩ | 1 | GND | GND |
| R24 | 100 kΩ | 2 | | Q7 gate |
| U9 | TLV62568DBVT | 4 VIN | VBUS | USB 5 V |
| U9 | TLV62568DBVT | 1 EN | VBUS | Held on from VBUS |
| U9 | TLV62568DBVT | 2 GND | GND | GND |
| U9 | TLV62568DBVT | 3 SW | | L3 pin 1 |
| U9 | TLV62568DBVT | 5 FB | | R20 / R21 midpoint |
| L3 | 2.2 µH | 1 | | U9 SW |
| L3 | 2.2 µH | 2 | | D9 anode |
| C16 | 10 µF | 1 | | D9 anode |
| C16 | 10 µF | 2 | GND | GND |
| C17 | 10 µF | 1 | | D9 anode |
| C17 | 10 µF | 2 | GND | GND |
| C29 | 4.7 µF | 1 | VBUS | |
| C29 | 4.7 µF | 2 | GND | GND |
| D9 | MBR0530T1G | 2 A | | L3 / C16 / C17 |
| D9 | MBR0530T1G | 1 K | +3V3 | +3V3 |
| R20 | 450 kΩ | 1 | +3V3 | +3V3 |
| R20 | 450 kΩ | 2 | | U9 FB |
| R21 | 100 kΩ | 1 | | U9 FB |
| R21 | 100 kΩ | 2 | GND | GND |
| C31 | 10 µF | 1 | +3V3 | +3V3 |
| C31 | 10 µF | 2 | GND | GND |

## ESP32

U1 is the ESP32-S3-WROOM-1-N8. Pin 2 is the module supply. Pins 1, 40, and 41 are ground. EN is pulled up to `+3V3` and shared with the boot button network. IO3 is pulled down so USB programming stays selected. IO0, IO45, and IO46 stay open.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U1 | ESP32-S3-WROOM-1-N8 | 2 3V3 | +3V3 | +3V3 |
| U1 | ESP32-S3-WROOM-1-N8 | 1, 40, 41 GND | GND | GND |
| U1 | ESP32-S3-WROOM-1-N8 | 3 EN | ESP_EN | R1 to +3V3; SW3 / R2 |
| U1 | ESP32-S3-WROOM-1-N8 | 15 IO3 | | R13 to GND |
| C3 | 10 µF | 1 | +3V3 | Module bulk |
| C3 | 10 µF | 2 | GND | GND |
| R13 | 10 kΩ | 1 | | ESP32 IO3 |
| R13 | 10 kΩ | 2 | GND | GND |

Every other used pad:

| Module pin | GPIO | Net | Other end |
|---|---|---|---|
| 4 | IO4 | BAT_ADC | R16 / R17 midpoint, C27 |
| 5 | IO5 | ENC_A | SW1 A, D5 |
| 6 | IO6 | ENC_B | SW1 B, D6 |
| 7 | IO7 | ENC_SW | SW1 S1, D7, C28 |
| 8 | IO15 | BACK | SW2, D4, C26 |
| 9 | IO16 | MOTOR | Q5 gate, R11 |
| 10 | IO17 | PIEZO | R12 |
| 11 | IO18 | GPS_UART_RX | U5 TXD |
| 12 | IO8 | RF_RST | U4 RST |
| 13 | GPIO19 | USB_D− | U8 |
| 14 | GPIO20 | USB_D+ | U8 |
| 17 | IO9 | RF_BUSY | U4 BUSY |
| 18 | IO10 | RF_CS | U4 NSS |
| 19 | IO11 | SPI_MOSI | U4 MOSI, U7 / J3 |
| 20 | IO12 | SPI_SCK | U4 SCK, U7 / J3 |
| 21 | IO13 | SPI_MISO | U4 MISO |
| 22 | IO14 | LCD_CS | U7 / J3 |
| 23 | IO21 | LCD_A0 | U7 / J3 |
| 24 | IO47 | LCD_RST | U7 / J3 |
| 28 | IO35 | LCD_BL | Q1 gate, R10 |
| 31 | IO38 | GPS_UART_TX | R18 toward U5 RXD |
| 32 | IO39 | GPS_PWR_N | Q2 gate, R9. A low turns GPS on |
| 33 | IO40 | GPS_RST | Q6 gate, R27. A high holds RESET_N low |
| 34 | IO41 | 5v_Detect | R25 / R26 midpoint, C32 |
| 38 | IO2 | I2C_SCL | U6 SCL, R14 to +3V3 |
| 39 | IO1 | I2C_SDA | U6 SDA, R15 to +3V3 |

Leave these module pins open: 16 IO46, 25 IO48, 26 IO45, 27 IO0, 29 IO36, 30 IO37, 35 IO42, 36 RXD0, 37 TXD0.

## LoRa radio

U4 is the HT-RA62. AE1 is the spring antenna. L2 is a 0 Ω link on the antenna path. DIO1–DIO3, TXEN, and RXEN stay open.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U4 | HT-RA62 | 3 3V3 | +3V3 | +3V3 |
| U4 | HT-RA62 | 2, 9, 16 GND | GND | GND |
| U4 | HT-RA62 | 4 RST | RF_RST | ESP32 U1 pin 12, IO8 |
| U4 | HT-RA62 | 10 BUSY | RF_BUSY | ESP32 U1 pin 17, IO9 |
| U4 | HT-RA62 | 15 NSS | RF_CS | ESP32 U1 pin 18, IO10 |
| U4 | HT-RA62 | 12 SCK | SPI_SCK | ESP32 IO12. Also screen |
| U4 | HT-RA62 | 14 MOSI | SPI_MOSI | ESP32 IO11. Also screen |
| U4 | HT-RA62 | 13 MISO | SPI_MISO | ESP32 IO13 |
| U4 | HT-RA62 | 1 ANT | | L2 |
| U4 | HT-RA62 | 5 TXEN, 6 DIO1, 7 DIO2, 8 DIO3, 11 RXEN | | Leave open |
| L2 | 0 Ω | 1 | | AE1 |
| L2 | 0 Ω | 2 | | U4 ANT |
| AE1 | T1-915M | 1 | | L2 |
| C14 | 10 µF | 1 | GND | GND |
| C14 | 10 µF | 2 | +3V3 | Radio bulk |
| C15 | 100 nF | 1 | GND | GND |
| C15 | 100 nF | 2 | +3V3 | Radio bypass |

## GPS

U5 is the Quectel LC86LICMD. VCC is on `GPS_VCC` from Q2. V_BCKP is on `+3V3`. A low on IO39 (`GPS_PWR_N`) turns Q2 on. MCU TX is `GPS_UART_TX` through R18 / R19 into U5 RXD. MCU RX is `GPS_UART_RX` from U5 TXD. RESET_N is the open drain of Q6; R27 pulls the gate down.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U5 | LC86LICMD | 4 VCC | GPS_VCC | From Q2 |
| U5 | LC86LICMD | 5 V_BCKP | +3V3 | Always-on backup |
| U5 | LC86LICMD | 3, 12 GND | GND | GND |
| U5 | LC86LICMD | 1 RXD | GPS_RXD | R18 / R19 midpoint |
| U5 | LC86LICMD | 2 TXD | GPS_UART_RX | ESP32 U1 pin 11, IO18 |
| U5 | LC86LICMD | 10 RESET_N | | Q6 drain |
| U5 | LC86LICMD | 6 1PPS, 7 WAKEUP, 8 AADET_N, 9 RESERVED, 11 EX_ANT | | Leave open |
| Q2 | AO3401A | 1 G | GPS_PWR_N | ESP32 IO39 |
| Q2 | AO3401A | 2 S | +3V3 | +3V3 |
| Q2 | AO3401A | 3 D | GPS_VCC | U5 VCC |
| R9 | 100 kΩ | 1 | +3V3 | +3V3 |
| R9 | 100 kΩ | 2 | GPS_PWR_N | |
| R18 | 1 kΩ | 1 | GPS_RXD | U5 RXD |
| R18 | 1 kΩ | 2 | GPS_UART_TX | ESP32 IO38 |
| R19 | 5.6 kΩ | 1 | GND | GND |
| R19 | 5.6 kΩ | 2 | GPS_RXD | |
| Q6 | 2N7002 | 1 G | GPS_RST | ESP32 IO40 |
| Q6 | 2N7002 | 2 S | GND | GND |
| Q6 | 2N7002 | 3 D | | U5 RESET_N |
| R27 | 10 kΩ | 1 | GPS_RST | |
| R27 | 10 kΩ | 2 | GND | GND |
| C18 | 100 nF | 1 | GND | GND |
| C18 | 100 nF | 2 | GPS_VCC | |
| C19 | 4.7 µF | 1 | GND | GND |
| C19 | 4.7 µF | 2 | GPS_VCC | |
| C20 | 33 pF | 1 | GND | GND |
| C20 | 33 pF | 2 | GPS_VCC | Closest to U5 |
| C21 | 100 nF | 1 | GND | GND |
| C21 | 100 nF | 2 | +3V3 | Near GPS |
| C22 | 4.7 µF | 1 | GND | GND |
| C22 | 4.7 µF | 2 | +3V3 | Near GPS |
| C23 | 33 pF | 1 | GND | GND |
| C23 | 33 pF | 2 | +3V3 | Near GPS |

## Compass

U6 is the LSM303AGRTR. Both chip-select pins are tied to `+3V3` (I2C mode). Interrupts stay open.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U6 | LSM303AGRTR | 9 VDD, 10 VDD_IO | +3V3 | +3V3 |
| U6 | LSM303AGRTR | 2 CS_XL, 3 CS_MAG | +3V3 | +3V3 |
| U6 | LSM303AGRTR | 6, 8 GND | GND | GND |
| U6 | LSM303AGRTR | 1 SCL | I2C_SCL | ESP32 IO2 |
| U6 | LSM303AGRTR | 4 SDA | I2C_SDA | ESP32 IO1 |
| U6 | LSM303AGRTR | 5 C1 | | C11 to GND |
| U6 | LSM303AGRTR | 7 INT_MAG, 11 INT2_XL, 12 INT1_XL | | Leave open |
| R14 | 4.7 kΩ | 1 | +3V3 | +3V3 |
| R14 | 4.7 kΩ | 2 | I2C_SCL | |
| R15 | 4.7 kΩ | 1 | I2C_SDA | |
| R15 | 4.7 kΩ | 2 | +3V3 | +3V3 |
| C11 | 220 nF | 1 | GND | GND |
| C11 | 220 nF | 2 | | U6 C1 |
| C12 | 10 µF | 1 | +3V3 | |
| C12 | 10 µF | 2 | GND | GND |
| C13 | 100 nF | 1 | +3V3 | |
| C13 | 100 nF | 2 | GND | GND |

## Screen

U7 is the ERC12864FSF-9 glass. J3 is the Molex 200528-0140 FPC socket on the board. SPI is shared with the radio. BLA is on `+3V3`. BLK is switched low by Q1. A high on IO35 turns the lamp on.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U7 / J3 | ERC12864FSF-9 / 200528-0140 | 1 BLA | +3V3 | +3V3 |
| U7 / J3 | | 8 VDD | +3V3 | +3V3 |
| U7 / J3 | | 9 VSS | GND | GND |
| U7 / J3 | | 2 BLK | LCD_BLK | Q1 drain |
| U7 / J3 | | 3 CS | LCD_CS | ESP32 IO14 |
| U7 / J3 | | 4 RST | LCD_RST | ESP32 IO47 |
| U7 / J3 | | 5 A0 | LCD_A0 | ESP32 IO21 |
| U7 / J3 | | 6 SCL | SPI_SCK | ESP32 IO12. Also U4 |
| U7 / J3 | | 7 SDA | SPI_MOSI | ESP32 IO11. Also U4 |
| U7 / J3 | | 10 V0, 11 XV0, 12 NC, 13 NC, 14 VG | | Leave open |
| Q1 | BSS138 | 1 G | LCD_BL | ESP32 IO35 |
| Q1 | BSS138 | 2 S | GND | GND |
| Q1 | BSS138 | 3 D | LCD_BLK | |
| R10 | 100 kΩ | 1 | GND | GND |
| R10 | 100 kΩ | 2 | LCD_BL | |
| C24 | 100 nF | 1 | +3V3 | |
| C24 | 100 nF | 2 | GND | GND |
| C25 | 100 nF | 1 | GND | GND |
| C25 | 100 nF | 2 | +3V3 | |

## Switches

SW1 is the PEC11R encoder. SW2 is Back. Commons go to ground. ESD diodes sit on the signal lines; C26 / C28 are local bypasses on BACK and ENC_SW.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| SW1 | PEC11R-4220F-S0024 | A | ENC_A | ESP32 IO5 |
| SW1 | PEC11R-4220F-S0024 | B | ENC_B | ESP32 IO6 |
| SW1 | PEC11R-4220F-S0024 | S1 | ENC_SW | ESP32 IO7 |
| SW1 | PEC11R-4220F-S0024 | C, S2 | GND | GND |
| SW2 | Back | 1 | GND | GND |
| SW2 | Back | 2 | BACK | ESP32 IO15 |
| D4 | ESD5Zxx | 1 | BACK | |
| D4 | ESD5Zxx | 2 | GND | GND |
| D5 | ESD5Zxx | 1 | ENC_A | |
| D5 | ESD5Zxx | 2 | GND | GND |
| D6 | ESD5Zxx | 1 | ENC_B | |
| D6 | ESD5Zxx | 2 | GND | GND |
| D7 | ESD5Zxx | 1 | ENC_SW | |
| D7 | ESD5Zxx | 2 | GND | GND |
| C26 | 100 nF | 1 | GND | GND |
| C26 | 100 nF | 2 | BACK | |
| C28 | 100 nF | 1 | GND | GND |
| C28 | 100 nF | 2 | ENC_SW | |

## Motor

M1 is the coin motor. Q5 is an AO3400A low-side switch. A high on IO16 runs the motor.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| M1 | LCM1027A2445F | 1 + | +3V3 | +3V3 |
| M1 | LCM1027A2445F | 2 − | | Q5 drain, D1 anode |
| Q5 | AO3400A | 1 G | MOTOR | ESP32 IO16 |
| Q5 | AO3400A | 2 S | GND | GND |
| Q5 | AO3400A | 3 D | | Motor negative |
| R11 | 100 kΩ | 1 | MOTOR | |
| R11 | 100 kΩ | 2 | GND | GND |
| D1 | 1N4148 | 1 K | +3V3 | +3V3 |
| D1 | 1N4148 | 2 A | | Motor negative |

## Piezo

BZ1 is the 10 mm piezo. The ESP32 pin goes through 100 Ω to the positive terminal.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| BZ1 | GSC1030RB-3V4000 | 1 + | | R12 |
| BZ1 | GSC1030RB-3V4000 | 2 − | GND | GND |
| R12 | 100 Ω | 1 | PIEZO | ESP32 IO17 |
| R12 | 100 Ω | 2 | | Piezo positive |
