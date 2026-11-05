# PCB connections

Connections on `hardware/pcb/pcb.kicad_sch`. Each section is one circuit, and every part in that circuit is in its table. The callout column is a pin that has to reach the ESP32, the 3.3 V rail (`+3V3`), or ground. A blank callout is a wire that stays inside the circuit.

`+3V3` is the buck output. `BAT` is the cell. `VBUS` is the USB 5 V input and feeds the charger only. `GPS_VCC` is the switched rail for the GPS module. The ESP32 runs from `+3V3`.

## USB-C

J1 is the GCT USB4105-GF-A. Both sides of the receptacle are tied together. VBUS goes to the charger. D+ and D− go to the ESP32 USB pins.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| J1 | USB4105-GF-A | A1, A12, B1, B12 GND | GND | GND |
| J1 | USB4105-GF-A | SHIELD | GND | GND |
| J1 | USB4105-GF-A | A4, A9, B4, B9 VBUS | VBUS | Charger U2 VDD |
| J1 | USB4105-GF-A | A6, B6 D+ | USB_D+ | ESP32 U1 pin 14, USB_D+, GPIO20 |
| J1 | USB4105-GF-A | A7, B7 D− | USB_D− | ESP32 U1 pin 13, USB_D−, GPIO19 |
| J1 | USB4105-GF-A | A5 CC1 | CC1 | R17 to GND |
| J1 | USB4105-GF-A | B5 CC2 | CC2 | R18 to GND |
| J1 | USB4105-GF-A | A8 SBU1, B8 SBU2 | | Leave open |
| R17 | 5.1 kΩ | 1 | CC1 | |
| R17 | 5.1 kΩ | 2 | GND | GND |
| R18 | 5.1 kΩ | 1 | CC2 | |
| R18 | 5.1 kΩ | 2 | GND | GND |

## Battery connector

J2 is the JST PH battery header. Pin 1 is the cell positive. The divider lets the ESP32 read the cell. The midpoint is half the cell voltage, so a full cell at 4.2 V puts 2.1 V on the ADC pin.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| J2 | S2B-PH-SM4-TB | 1 | BAT | Cell positive. Buck input and charger output |
| J2 | S2B-PH-SM4-TB | 2 | GND | GND |
| R14 | 100 kΩ | 1 | BAT | |
| R14 | 100 kΩ | 2 | BAT_ADC | ESP32 U1 pin 4, IO4 |
| R15 | 100 kΩ | 1 | BAT_ADC | ESP32 U1 pin 4, IO4 |
| R15 | 100 kΩ | 2 | GND | GND |
| C14 | 100 nF | 1 | BAT_ADC | ESP32 U1 pin 4, IO4 |
| C14 | 100 nF | 2 | GND | GND |

## Charger

U2 is the MCP73831T-2ACI/OT, the 4.20 V version. PROG is 2.5 kΩ, which sets 400 mA. STAT has nowhere to go.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U2 | MCP73831T-2ACI/OT | 4 VDD | VBUS | USB 5 V |
| U2 | MCP73831T-2ACI/OT | 3 VBAT | BAT | Cell, J2 pin 1 |
| U2 | MCP73831T-2ACI/OT | 2 VSS | GND | GND |
| U2 | MCP73831T-2ACI/OT | 5 PROG | CHG_PROG | R13 to GND |
| U2 | MCP73831T-2ACI/OT | 1 STAT | | Leave open |
| R13 | 2.5 kΩ | 1 | CHG_PROG | |
| R13 | 2.5 kΩ | 2 | GND | GND |
| C9 | 4.7 µF | 1 | VBUS | USB 5 V |
| C9 | 4.7 µF | 2 | GND | GND |
| C10 | 4.7 µF | 1 | BAT | Cell |
| C10 | 4.7 µF | 2 | GND | GND |

## Buck

U3 is the TLV62568DBVT. EN is tied to VIN, so the regulator stays on while a cell is connected. The switch node goes through the inductor to `+3V3`. The feedback pair sets that rail near 3.31 V. C8 is the bulk capacitor on the same rail and sits beside the radio.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U3 | TLV62568DBVT | 4 VIN | BAT | Cell |
| U3 | TLV62568DBVT | 1 EN | BAT | Cell. Held on |
| U3 | TLV62568DBVT | 2 GND | GND | GND |
| U3 | TLV62568DBVT | 3 SW | BUCK_SW | L1 pin 1 |
| U3 | TLV62568DBVT | 5 FB | BUCK_FB | R1 / R2 midpoint |
| L1 | 2.2 µH | 1 | BUCK_SW | |
| L1 | 2.2 µH | 2 | +3V3 | +3V3 |
| R1 | 100 kΩ | 1 | +3V3 | +3V3 |
| R1 | 100 kΩ | 2 | BUCK_FB | |
| R2 | 22.1 kΩ | 1 | BUCK_FB | |
| R2 | 22.1 kΩ | 2 | GND | GND |
| C1 | 10 µF | 1 | BAT | Cell side of the buck |
| C1 | 10 µF | 2 | GND | GND |
| C2 | 10 µF | 1 | +3V3 | +3V3 |
| C2 | 10 µF | 2 | GND | GND |
| C8 | 10 µF | 1 | +3V3 | +3V3 |
| C8 | 10 µF | 2 | GND | GND |

## ESP32

U1 is the ESP32-S3-WROOM-1-N8. Pin 2 is the module supply. Pins 1, 40, and 41 are ground. EN is pulled up and has to stay that way. IO3 is pulled down so USB programming stays selected. IO0, IO45, and IO46 stay open.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U1 | ESP32-S3-WROOM-1-N8 | 2 3V3 | +3V3 | +3V3 |
| U1 | ESP32-S3-WROOM-1-N8 | 1, 40, 41 GND | GND | GND |
| U1 | ESP32-S3-WROOM-1-N8 | 3 EN | ESP_EN | R16 to +3V3 |
| U1 | ESP32-S3-WROOM-1-N8 | 15 IO3 | ESP_IO3 | R20 to GND |
| R16 | 10 kΩ | 1 | ESP_EN | ESP32 EN |
| R16 | 10 kΩ | 2 | +3V3 | +3V3 |
| R20 | 10 kΩ | 1 | ESP_IO3 | ESP32 IO3 |
| R20 | 10 kΩ | 2 | GND | GND |

Every other used pad:

| Module pin | GPIO | Net | Other end |
|---|---|---|---|
| 4 | IO4 | BAT_ADC | R14 / R15 midpoint, C14 |
| 5 | IO5 | ENC_A | SW1 A |
| 6 | IO6 | ENC_B | SW1 B |
| 7 | IO7 | ENC_SW | SW1 S1 |
| 8 | IO15 | BACK | SW2 pin 1 |
| 9 | IO16 | MOTOR | Q5 gate |
| 10 | IO17 | PIEZO | R8, then the piezo |
| 11 | IO18 | GPS_TXD | U5 TXD, direct |
| 12 | IO8 | RF_RST | U4 RST |
| 13 | GPIO19 | USB_D− | J1 D− |
| 14 | GPIO20 | USB_D+ | J1 D+ |
| 17 | IO9 | RF_BUSY | U4 BUSY |
| 18 | IO10 | RF_CS | U4 NSS |
| 19 | IO11 | SPI_MOSI | U4 MOSI and U7 pin 7 |
| 20 | IO12 | SPI_SCK | U4 SCK and U7 pin 6 |
| 21 | IO13 | SPI_MISO | U4 MISO |
| 22 | IO14 | LCD_CS | U7 CS |
| 23 | IO21 | LCD_A0 | U7 A0 |
| 24 | IO47 | LCD_RST | U7 RST |
| 28 | IO35 | LCD_BL | Q1 gate. A high turns the lamp on |
| 31 | IO38 | GPS_RXD | R21. The GPS pin is on the other side of the divider |
| 32 | IO39 | GPS_EN | Q4 gate. A low turns the GPS on |
| 33 | IO40 | GPS_RST | Q6 gate. A high holds RESET_N low |
| 38 | IO2 | I2C_SCL | U6 SCL, R10 to +3V3 |
| 39 | IO1 | I2C_SDA | U6 SDA, R9 to +3V3 |

Leave these module pins open: 16 IO46, 25 IO48, 26 IO45, 27 IO0, 29 IO36, 30 IO37, 34 IO41, 35 IO42, 36 RXD0, 37 TXD0.

## LoRa radio

U4 is the HT-RA62. The spring is the antenna and solders to the ANT pad. DIO2 stays open; it steers the antenna switch inside the module. The clock and the data-out lines are shared with the screen. MISO is the radio alone. C6 is the local bypass. The rail bulk capacitor is C8, listed with the buck.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U4 | HT-RA62 | 3 3V3 | +3V3 | +3V3 |
| U4 | HT-RA62 | 2, 9, 16 GND | GND | GND |
| U4 | HT-RA62 | 4 RST | RF_RST | ESP32 U1 pin 12, IO8 |
| U4 | HT-RA62 | 10 BUSY | RF_BUSY | ESP32 U1 pin 17, IO9 |
| U4 | HT-RA62 | 15 NSS | RF_CS | ESP32 U1 pin 18, IO10 |
| U4 | HT-RA62 | 12 SCK | SPI_SCK | ESP32 U1 pin 20, IO12. Also U7 pin 6 |
| U4 | HT-RA62 | 14 MOSI | SPI_MOSI | ESP32 U1 pin 19, IO11. Also U7 pin 7 |
| U4 | HT-RA62 | 13 MISO | SPI_MISO | ESP32 U1 pin 21, IO13 |
| U4 | HT-RA62 | 1 ANT | RF_ANT | AE1 |
| U4 | HT-RA62 | 5 TXEN, 6 DIO1, 7 DIO2, 8 DIO3, 11 RXEN | | Leave open |
| AE1 | T1-915M | 1 | RF_ANT | U4 ANT |
| C6 | 100 nF | 1 | +3V3 | +3V3 |
| C6 | 100 nF | 2 | GND | GND |

## GPS

U5 is the Quectel LC86LICMD. VCC and V_BCKP are both on `GPS_VCC`. That rail comes from `+3V3` through Q4. A low on IO39 turns the module on, and R6 holds the gate at 3.3 V until the pin is driven. IO38 reaches RXD through the 1 kΩ / 5.6 kΩ divider, so the module pin stays under its 3.1 V high. TXD comes back to IO18 with no divider. RESET_N is the open drain of Q6, and that pin has no pull-up. The 33 pF sits closest to the module.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U5 | LC86LICMD | 4 VCC | GPS_VCC | Switched 3.3 V, from Q4 |
| U5 | LC86LICMD | 5 V_BCKP | GPS_VCC | Same switched rail |
| U5 | LC86LICMD | 3, 12 GND | GND | GND |
| U5 | LC86LICMD | 1 RXD | GPS_RX | R21 / R22 midpoint. ESP32 IO38 is on the other side of R21 |
| U5 | LC86LICMD | 2 TXD | GPS_TXD | ESP32 U1 pin 11, IO18 |
| U5 | LC86LICMD | 10 RESET_N | GPS_RST_N | Q6 drain. ESP32 IO40 drives the gate |
| U5 | LC86LICMD | 6 1PPS, 7 WAKEUP, 8 AADET_N, 9 RESERVED, 11 EX_ANT | | Leave open |
| Q4 | AO3401A | 1 G | GPS_EN | ESP32 U1 pin 32, IO39 |
| Q4 | AO3401A | 2 S | +3V3 | +3V3 |
| Q4 | AO3401A | 3 D | GPS_VCC | U5 VCC and V_BCKP |
| R6 | 100 kΩ | 1 | GPS_EN | ESP32 IO39 |
| R6 | 100 kΩ | 2 | +3V3 | +3V3 |
| R21 | 1 kΩ | 1 | GPS_RXD | ESP32 U1 pin 31, IO38 |
| R21 | 1 kΩ | 2 | GPS_RX | U5 RXD |
| R22 | 5.6 kΩ | 1 | GPS_RX | U5 RXD |
| R22 | 5.6 kΩ | 2 | GND | GND |
| Q6 | 2N7002 | 1 G | GPS_RST | ESP32 U1 pin 33, IO40 |
| Q6 | 2N7002 | 2 S | GND | GND |
| Q6 | 2N7002 | 3 D | GPS_RST_N | U5 RESET_N |
| R19 | 100 kΩ | 1 | GPS_RST | ESP32 IO40 |
| R19 | 100 kΩ | 2 | GND | GND |
| C11 | 4.7 µF | 1 | GPS_VCC | Switched rail |
| C11 | 4.7 µF | 2 | GND | GND |
| C12 | 100 nF | 1 | GPS_VCC | Switched rail |
| C12 | 100 nF | 2 | GND | GND |
| C13 | 33 pF | 1 | GPS_VCC | Switched rail, closest to U5 |
| C13 | 33 pF | 2 | GND | GND |

## Compass

U6 is the LSM303AGRTR. Both chip-select pins are tied to `+3V3`, which holds the part in I2C mode. The two supply pins are on the same rail. C1 on the part goes to ground through C3. The interrupt pins stay open.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U6 | LSM303AGRTR | 9 VDD, 10 VDD_IO | +3V3 | +3V3 |
| U6 | LSM303AGRTR | 2 CS_XL, 3 CS_MAG | +3V3 | +3V3 |
| U6 | LSM303AGRTR | 6, 8 GND | GND | GND |
| U6 | LSM303AGRTR | 1 SCL | I2C_SCL | ESP32 U1 pin 38, IO2 |
| U6 | LSM303AGRTR | 4 SDA | I2C_SDA | ESP32 U1 pin 39, IO1 |
| U6 | LSM303AGRTR | 5 C1 | LSM_C1 | C3 to GND |
| U6 | LSM303AGRTR | 7 INT_MAG, 11 INT2_XL, 12 INT1_XL | | Leave open |
| R10 | 4.7 kΩ | 1 | I2C_SCL | ESP32 IO2 |
| R10 | 4.7 kΩ | 2 | +3V3 | +3V3 |
| R9 | 4.7 kΩ | 1 | I2C_SDA | ESP32 IO1 |
| R9 | 4.7 kΩ | 2 | +3V3 | +3V3 |
| C3 | 220 nF | 1 | LSM_C1 | U6 C1 |
| C3 | 220 nF | 2 | GND | GND |
| C4 | 100 nF | 1 | +3V3 | +3V3 |
| C4 | 100 nF | 2 | GND | GND |
| C5 | 100 nF | 1 | +3V3 | +3V3 |
| C5 | 100 nF | 2 | GND | GND |

## Screen

U7 is the ERC12864FSF-9 flex. Pin 6 is named SCL on the glass and pin 7 is named SDA, and both are the SPI bus shared with the radio, the same nets as `SPI_SCK` and `SPI_MOSI`. BLA is the lamp anode and is tied to `+3V3`. The lamp cathode is a low-side switch. A high on IO35 turns the lamp on. V0, XV0, and VG stay open. The glass has its own LED resistor.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| U7 | ERC12864FSF-9 | 1 BLA | +3V3 | +3V3 |
| U7 | ERC12864FSF-9 | 8 VDD | +3V3 | +3V3 |
| U7 | ERC12864FSF-9 | 9 VSS | GND | GND |
| U7 | ERC12864FSF-9 | 2 BLK | LCD_BLK | Q1 drain, then GND |
| U7 | ERC12864FSF-9 | 3 CS | LCD_CS | ESP32 U1 pin 22, IO14 |
| U7 | ERC12864FSF-9 | 4 RST | LCD_RST | ESP32 U1 pin 24, IO47 |
| U7 | ERC12864FSF-9 | 5 A0 | LCD_A0 | ESP32 U1 pin 23, IO21 |
| U7 | ERC12864FSF-9 | 6 SCL | SPI_SCK | ESP32 U1 pin 20, IO12. Also U4 SCK |
| U7 | ERC12864FSF-9 | 7 SDA | SPI_MOSI | ESP32 U1 pin 19, IO11. Also U4 MOSI |
| U7 | ERC12864FSF-9 | 10 V0, 11 XV0, 12 NC, 13 NC, 14 VG | | Leave open |
| Q1 | BSS138 | 1 G | LCD_BL | ESP32 U1 pin 28, IO35 |
| Q1 | BSS138 | 2 S | GND | GND |
| Q1 | BSS138 | 3 D | LCD_BLK | U7 BLK |
| R3 | 100 kΩ | 1 | LCD_BL | ESP32 IO35 |
| R3 | 100 kΩ | 2 | GND | GND |
| C7 | 100 nF | 1 | +3V3 | +3V3 |
| C7 | 100 nF | 2 | GND | GND |

## Switches

SW1 is the PEC11R encoder. SW2 is the back button. Neither has an external resistor. Each signal pin goes to the ESP32, and each common goes to ground.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| SW1 | PEC11R-4220F-S0024 | A | ENC_A | ESP32 U1 pin 5, IO5 |
| SW1 | PEC11R-4220F-S0024 | B | ENC_B | ESP32 U1 pin 6, IO6 |
| SW1 | PEC11R-4220F-S0024 | S1 | ENC_SW | ESP32 U1 pin 7, IO7 |
| SW1 | PEC11R-4220F-S0024 | C, S2 | GND | GND |
| SW2 | Back | 1 | BACK | ESP32 U1 pin 8, IO15 |
| SW2 | Back | 2 | GND | GND |

## Motor

M1 is the coin motor. The positive terminal is on `+3V3`. Q5 is the low-side switch, and a high on IO16 runs the motor. The diode cathode is on `+3V3` and the anode is on the motor negative.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| M1 | LCM1027A2445F | 1 + | +3V3 | +3V3 |
| M1 | LCM1027A2445F | 2 − | MOTOR_N | Q5 drain |
| Q5 | 2N7002 | 1 G | MOTOR | ESP32 U1 pin 9, IO16 |
| Q5 | 2N7002 | 2 S | GND | GND |
| Q5 | 2N7002 | 3 D | MOTOR_N | Motor negative |
| R7 | 100 kΩ | 1 | MOTOR | ESP32 IO16 |
| R7 | 100 kΩ | 2 | GND | GND |
| D1 | 1N4148 | 1 K | +3V3 | +3V3 |
| D1 | 1N4148 | 2 A | MOTOR_N | Motor negative |

## Piezo

BZ1 is the 10 mm piezo. The ESP32 pin goes through 100 Ω to the positive terminal. The other terminal is ground.

| Ref | Part | Pin | Net | Callout |
|---|---|---|---|---|
| BZ1 | GSC1030RB-3V4000 | 1 + | PIEZO_P | R8 from the ESP32 |
| BZ1 | GSC1030RB-3V4000 | 2 − | GND | GND |
| R8 | 100 Ω | 1 | PIEZO | ESP32 U1 pin 10, IO17 |
| R8 | 100 Ω | 2 | PIEZO_P | Piezo positive |
