# Build one prototype beeper

This is the wiring checklist for a single bench unit. It assumes you have never assembled a circuit. Each section tells you what to connect, why that connection exists, and what you should see before you continue.

You are building one pager on a breadboard: a Feather nRF52840, a 915 MHz SX1262 radio, a 128×64 FSTN screen, a knob, a back button, a vibration motor, a piezo, a GPS, an LSM303AGR compass, and a LiPo. The three demo units are three copies of this same checklist.

The Feather already contains a small program from Adafruit called a bootloader. That program only lets the Mac see the board. The pager firmware in this repository does not yet know these pins. When the checklist is done, the screen stays blank, the radio stays quiet, and the GPS has not been asked for a position. What you have proved is that power is right, nothing is shorted, and every wire lands on the pin the firmware port will use.

Do the steps in order. Do not skip ahead to the battery.

## Words this guide uses

A **pin** is a metal leg, or a hole tied to one, with a name printed beside it. That printing is the board's labels. Read the label that sits against the hole you are about to use. The neighbor's label belongs to the neighbor.

A **wire** joins two holes that are supposed to share a signal. A **rail** is a long strip of holes that are all connected, used here for power and ground. A **bus** is a few shared wires plus one private wire per part that means "I am talking to you, not to the other part."

A **breadboard** hides those connections. The holes in one short row, on one side of the center trench, are tied together. The holes straight across the trench are a different row. The colored stripes along the sides are the rails. If a stripe has a break in the middle of the board, the two halves are not connected.

## What you are holding

| Part | What it does on this pager |
|---|---|
| Adafruit Feather nRF52840 Express | The computer. It will run the pager program. It also makes a safe 3.3 volt supply from USB or the battery. |
| Waveshare Pico-LoRa-SX1262-915M | The long-range radio. The chip on it is an SX1262. The board is shaped for a Raspberry Pi Pico, which we do not use. We use the pin names printed on it. |
| BuyDisplay ERM12864FSF-9 | The screen. 128 dots wide, 64 tall, gray FSTN glass, ST7567 controller. |
| Rotary encoder with knob | The only control for menus. Turning it moves. Pressing the knob selects. |
| Tactile button | Back. Separate from the knob so you can leave a screen without turning anything. |
| Vibration motor disc, PN2222 transistor, 1N4148 diode, 1 kΩ resistor | The buzzer you feel. The transistor is a switch, because the motor wants more current than a Feather pin can give. |
| Piezo disc (PS1240) and a 100 Ω resistor | The buzzer you hear. |
| Ultimate GPS breakout and a CR1220 coin cell | Position, and a clock that stays warm when the pager sleeps. Firmware can turn it off. |
| Adafruit LSM303AGR breakout | Which way is down, and which way is north when the board is held flat. The chip does not correct a tipped heading. |
| Adafruit 1200 mAh LiPo | The battery. Its plug matches the Feather. The battery that came with the radio does not. |

Two voltages show up on the Feather. The pin named **EN** is not a voltage. Grounding **EN** turns the 3.3 volt regulator off. Leave **EN** empty.

| Feather label | What it is | Use it for |
|---|---|---|
| **3V** | A regulated 3.3 volt output. This is the only supply the radio, screen, GPS, compass, and motor are allowed to use. | Everything in this guide except the USB cable and the LiPo plug. |
| **GND** | Ground. The zero-volt reference. Every part needs a ground wire, or its other wires mean nothing. | Every module. |
| **USB** | About 5 volts, and only while the USB cable is plugged in. | Do not connect this pin to anything in this guide. |
| **BAT** | The raw battery, about 3.5 to 4.2 volts. | Do not connect this pin to anything in this guide. The battery itself plugs into the white socket on the Feather. |

3.3 volts on a pin that expects 5 volts usually just fails to run. 5 volts on a pin that expects 3.3 volts punches through the chip. The screen's logic supply is rated to 3.6 volts. The radio chip is a 3.3 volt part. Stay on the Feather's **3V** pin.

## Rules before any wire

- [ ] Unplug USB.
- [ ] Unplug the LiPo. Leave it in the charging bag until the last section.
- [ ] Leave the radio's own small battery in its bag. Its plug is a different shape. Do not try it in the Feather's socket.
- [ ] Screw the 915 MHz antenna onto the radio before the radio ever receives power. A transmitter with no antenna reflects energy back into the chip. We will not transmit today. The antenna still goes on now so that step cannot be forgotten later.
- [ ] Wash your hands after soldering. The solder in the Adafruit spool is 60/40 tin-lead.
- [ ] Safety glasses on while the iron is hot and while you clip leads.

If a step says "stop," unplug USB before you move a wire.

## Pin map

This is the contract. Later firmware should use these Feather pins. Pins not in the table stay empty.

The radio's signal names (`GP10`, `3V3`, and so on) are printed on the radio board. They are Pico names. You are not plugging in a Pico.

| Function | Feather pin | Other end |
|---|---|---|
| Radio and screen clock | **SCK** | Radio `GP10`, and screen pin 8 `SCLK` |
| Radio and screen data out | **MO** | Radio `GP11`, and screen pin 9 `SID` |
| Radio data in | **MI** | Radio `GP12` only |
| Radio chip select | **D10** | Radio `GP3` |
| Radio busy | **D11** | Radio `GP2` |
| Radio reset | **D12** | Radio `GP15` only |
| Radio interrupt | **D13** | Radio `GP20` |
| Screen command/data | **A0** | Screen pin 10 `A0` |
| Screen reset | **A1** | Screen pin 11 `/RST` |
| Screen chip select | **A2** | Screen pin 12 `/CS` |
| Knob A | **D6** | Encoder pin beside the middle, with a 10 kΩ pull-up |
| Knob B | **D9** | The other pin beside the middle, with a 10 kΩ pull-up |
| Knob press | **A3** | Encoder switch, with a 10 kΩ pull-up |
| Back button | **A4** | Button, with a 10 kΩ pull-up |
| Vibration | **A5** | 1 kΩ resistor, then the transistor base |
| Beep | **D2** | 100 Ω resistor, then the piezo |
| GPS receive | **RX** | GPS **TX** |
| GPS transmit | **TX** | GPS **RX** |
| GPS enable | **D5** | GPS **EN** |
| Compass data | **SDA** | Compass **SDA** |
| Compass clock | **SCL** | Compass **SCL** |

Leave the Feather pins **USB**, **BAT**, **EN**, and **AREF** empty. **AREF** is an analog reference, not a spare button pin. The GPS **PPS** pin stays empty too. **D5** is the GPS enable, so the once-per-second pulse has nowhere to land. Time still comes from the GPS sentences while the receiver is on.

**D12** is the radio reset, active low, and only the radio. The compass has no reset pin. The board port pulses **D12** once at boot, and again only when the radio has stopped answering. The code in this repository does not pulse that pin yet. Changing transmit power, or switching the radio between send and receive, must not pulse this wire. A pulse aborts whatever the radio was sending.

Several names you may see in pin diagrams are not holes on the breadboard header. **D3** is the onboard red LED. **D4** is the onboard blue LED. **D7** is the small button already on the Feather. **D8** is the onboard color LED. **A6** is tied to the battery divider inside the board. Do not hunt for those holes.

Do not wire radio `GP22`. On a stock board the antenna switch is steered by the SX1262's own DIO2 pin. `GP22` connects to that switch only if solder jumper H3 is moved from position A to position B. Leave the jumper where the factory put it, and leave `GP22` empty.

## 1. Solder the headers

The Feather, the GPS, and the compass arrive with loose pin strips. The screen should already have its 12-pin header because that option was selected when you ordered it. The radio already has sockets.

A male header is the loose strip of pins. The long ends go down into the breadboard. The short ends poke up through the board you are soldering.

- [ ] Count the holes on each long edge of the Feather. One edge has 12 holes. The other has 16. Break the strips to those two lengths. A pin that hangs past the last hole is in the wrong place. Pull it off before you solder.
- [ ] Push the long pins into the breadboard so the plastic sits on the breadboard. Lower the Feather onto the short ends. Each short end comes up through a hole on the top of the Feather.
- [ ] Touch the iron to one short pin and the metal ring around it. Feed solder onto the ring, not onto the iron tip. Stop when that joint is a small shiny cone. Do the next pin the same way. If solder joins two neighboring pins, heat that bridge and clear it before the next pin.
- [ ] Solder the GPS the same way. The printed names VIN, GND, TX, and RX face up. Count that row and use a strip of that length only.
- [ ] Solder the compass the same way. The printed names VIN, GND, SDA, and SCL face up. Count that row and use a strip of that length only.
- [ ] On the encoder, clip only the two wide flat ears on the sides of the metal can. Leave every thin pin. Those ears are what Adafruit means by the side tabs.
- [ ] Meter on continuity (it beeps when the probes touch each other, or touch two holes that are already connected). Touch one probe to the Feather pin labeled **3V** and the other to the Feather pin labeled **GND**. Silence is correct. A beep means solder is joining those two pins. Clear that bridge before USB.

## 2. Power the Feather alone

- [ ] Plug a USB-C to Micro-B **data** cable into the Feather and into the Mac. No other wires.
- [ ] Set the meter to DC volts. Black probe on **GND**, red probe on **3V**. You want about 3.3 volts. Anything above 3.6 or below 3.0 means stop.
- [ ] Double-press the Feather's reset button, two quick taps. A drive named `FTHR840BOOT` should appear in Finder. Eject it. That drive is the bootloader, not the pager. If no drive appears, try a different cable. Many cables are charge-only and have no data wires.

Unplug USB before the next section.

## 3. Give the breadboard a 3.3 volt rail

- [ ] Seat the Feather so it straddles the center trench, one header on each side. Each pin owns the short group of holes beside it, on that same side of the trench. The holes across the trench are a different connection. When a later step names a Feather pin, use an empty hole in that pin's group. Leave the hole the pin itself already fills.
- [ ] Run one wire from Feather **3V** to a hole in the red stripe beside the Feather. Run one wire from Feather **GND** to the blue stripe next to that red stripe. Add another ground wire if you want. Never run a wire from the red stripe to the blue stripe.
- [ ] If the red line is broken at the middle of the board, the two halves are separate. Put the Feather's 3V wire and every later red-rail wire in the same half, or bridge the break with one wire. Do the same for the blue stripe.
- [ ] Plug USB in. Black probe in that blue stripe, red probe in that red stripe. About 3.3 volts. Unplug USB.

Every later "red rail" and "blue rail" means these two stripes.

## 4. Radio

Read the white name printed beside each socket. That name belongs only to the hole it sits against. `GP10` is the socket whose own printing says GP10.

The socket printed **3V3** is an input on this build. With a Pico plugged in, the Pico would make 3.3 volts and feed this pin. There is no Pico, so the Feather's red rail feeds it. That is what powers the SX1262. The socket printed **3V3_EN** is the neighbor and gets no wire.

**VBUS** and **VSYS** get no wire. They belong to the onboard charger. Do not plug the radio's own LiPo into the radio either.

The antenna switch is already handled on the board. The SX1262's DIO2 pin steers it. You do not add a wire for that.

- [ ] Antenna is attached.
- [ ] Radio **3V3** to the red rail.
- [ ] A radio **GND** to the blue rail. Any socket printed GND is the same connection.
- [ ] `GP10` to an empty hole in the Feather **SCK** group.
- [ ] `GP11` to an empty hole in the Feather **MO** group.
- [ ] `GP12` to an empty hole in the Feather **MI** group.
- [ ] `GP3` to Feather **D10**. This is chip select. The radio ignores the clock and data wires until this pin is low.
- [ ] `GP2` to Feather **D11**. The SX1262 holds this high while it is too busy to take another command.
- [ ] `GP15` to Feather **D12**. Reset, active low. Nothing else lands in this hole. Do not tie it to ground.
- [ ] `GP20` to Feather **D13**. The radio pulses this when a packet arrives.
- [ ] `GP22`, `GP26`, **VBUS**, **VSYS**, and **3V3_EN** have no wires.

Plug USB in. Black probe on a radio socket printed GND, red probe on the socket printed **3V3**. About 3.3 volts. The PWR LED on the radio should light, because that LED sits on this 3.3 volt pin. The CHG LED stays dark. That one belongs to the charger we are not using. Unplug USB.

## 5. Screen

Find the printed **1** beside the screen header. That hole is pin 1. The hole at the other end is pin 12. Pin 3 and pin 8 are both labeled SCLK. They are different pins. Pin 3 stays empty.

Pins 1–4 belong to an optional font chip this build does not use. A wire on them can put a second chip on the shared data lines.

| Screen pin | Name | Connect to |
|---|---|---|
| 1 | SI | no wire |
| 2 | S0 | no wire |
| 3 | SCLK (font chip) | no wire |
| 4 | CS# (font chip) | no wire |
| 5 | LEDA | red rail |
| 6 | VSS | blue rail |
| 7 | VDD | red rail |
| 8 | SCLK | an empty hole in the Feather **SCK** group, the group that already holds radio `GP10` |
| 9 | SID | an empty hole in the Feather **MO** group |
| 10 | A0 | Feather **A0** |
| 11 | /RST | Feather **A1** |
| 12 | /CS | Feather **A2** |

The screen and the radio share **SCK** and **MO**. That is normal. Only one of them is listened to at a time, chosen by its own chip-select pin (**D10** for the radio, **A2** for the screen). The screen has nothing to say back, so it does not connect to **MI**. Leave the radio's wires where they are. You are adding the screen onto the same rows.

Pin 5 powers the backlight. The module is rated to take 3.3 volts there and draw about 50 to 60 milliamps. A Feather signal pin cannot supply that, so pin 5 goes to the red rail.

- [ ] Wire the table. Pins 1, 2, 3, and 4 get no wire.
- [ ] USB in. Black probe on pin 6, red probe on pin 7. About 3.3 volts. The glass should be lit, even if the picture is blank or speckled. A dark glass means pin 5 or pin 6 is in the wrong hole.
- [ ] If the module is too hot to touch, or the red rail falls under 3.0 volts, unplug. Pin 7 has likely landed on the Feather pin labeled **USB**.

Unplug USB.

## 6. Knob and back button

The encoder has five thin pins. Three sit in one row. Two sit as a separate pair. The center pin of the three goes to ground. The two beside it are the rotation signals. Either one may be A. If menus move backward once firmware exists, swap the D6 and D9 wires. That swap is safe.

A **pull-up** is a resistor from the signal to the red rail. The switch shorts that signal to ground when it closes. When it is open, the resistor holds the pin at 3.3 volts so the Feather does not read a random in-between voltage. The 10 kΩ resistors have color bands brown, black, orange, gold. Set the 1 kΩ and 100 Ω parts aside until later sections.

- [ ] Center pin of the encoder's three-pin row to the blue rail.
- [ ] Either pin beside that center to an empty hole in the Feather **D6** group. The other to an empty hole in the Feather **D9** group.
- [ ] For each of **D6**, **D9**, **A3**, and **A4**: one end of a 10 kΩ resistor in that signal's hole group, the other end in the red rail. Both ends in the blue rail, or one end in red and one end in blue with nothing on the signal, is the wrong placement.
- [ ] One pin of the encoder's remaining pair to the blue rail. The other to an empty hole in the Feather **A3** group.
- [ ] The back button has four legs. The two legs on the same side are connected even when you are not pressing. Pick one leg from each side. Meter on continuity, USB unplugged: those two legs stay silent until you press, and beep while you press. Then one chosen leg goes to the blue rail and the other to an empty hole in the Feather **A4** group.

Plug USB in. Black probe on the blue rail, red probe on the **A4** group. About 3.3 volts, falling to about 0 while the button is held. If it stays at 0, the two legs you picked are the pair that is always connected, or the resistor went to the blue rail. Check **A3** the same way by pressing the knob. Unplug USB.

## 7. Vibration motor

A Feather pin is a signal, not a power supply for a motor. This motor draws on the order of tens of milliamps. Connecting it straight to **A5** damages the pin. The PN2222 is a switch. A small current into the middle leg lets a larger current flow from the right leg to the left leg.

Hold the flat face toward you with the legs pointing down. Left leg is the **emitter**, middle is the **base**, right is the **collector**. That order is for the PN2222 in the TO-92 package. If the bag is a different part number, stop and read its sheet. The order is not universal.

The 1N4148 diode goes across the motor, backward. A spinning motor is also a small generator. When it is switched off, that energy needs a loop through the motor instead of a spike into the transistor. The band on the diode is the end that points at the red rail.

The 1 kΩ resistor has bands brown, black, red, gold.

- [ ] Push the transistor in with the flat face still toward you, so you do not swap left and right after you set it down.
- [ ] Motor red wire into the red rail.
- [ ] Motor blue wire into an empty hole of the collector's group (the right leg).
- [ ] Emitter (left leg) into the blue rail. Use a short wire if the leg does not reach.
- [ ] 1 kΩ resistor from an empty hole in the Feather **A5** group to an empty hole in the base's group (the middle leg).
- [ ] Banded end of the diode into the red rail. Unbanded end into the collector's group, the same group as the motor's blue wire.

Plug USB in. The motor stays still. Leave it still. A wire from the motor's blue lead straight to Feather **A5** is the connection that damages the pin. Unplug USB.

Some motor discs use black instead of blue for the negative lead. The lead that is not red goes to the collector. If both leads are the same color, either orientation spins the motor. The diode's band still goes to the red rail.

## 8. Piezo

The PS1240 does not beep if you put steady 3.3 volts across it. It beeps when a pin toggles back and forth, which firmware will do later. It has no polarity. The 100 Ω resistor (bands brown, black, brown, gold) keeps the current pulse small.

- [ ] One piezo leg to the blue rail.
- [ ] The other piezo leg to one end of the 100 Ω resistor.
- [ ] The other end of that resistor to an empty hole in the Feather pin labeled **D2**. That hole is next to **TX**. The board also prints NFC2 by it. This build uses it as the beep pin.

The piezo stays silent. That is the right result.

## 9. GPS

Seat the GPS at the opposite end of the breadboard from the antenna before any of its wires go in. A later transmit is loud at 915 MHz and can knock a GPS fix loose if the two antennas sit on top of each other.

**VIN** is where this breakout takes power. The pin labeled **3.3V** is an output from the breakout's own regulator. Leave **3.3V** empty. Connecting it to the red rail ties two regulators together.

The names TX and RX belong to the chip printed next to them. The GPS sends on its TX pin. The Feather listens on its RX pin. The table is already crossed. Wire it as written. Do not cross it a second time.

| GPS pin | Connect to |
|---|---|
| VIN | red rail |
| GND | blue rail |
| TX | Feather **RX** |
| RX | Feather **TX** |
| EN | Feather **D5** |
| PPS | no wire |
| 3.3V | no wire |
| VBAT | no wire. The coin cell is the backup. |

The breakout pulls **EN** up to 3.3 volts, and that is what turns the receiver on. The Feather can later drive **D5** low to turn the GPS off. Do not wire **EN** to the blue rail. That holds the receiver off. **PPS** stays empty on purpose.

- [ ] Slide a CR1220 into the holder on the back, so the face stamped **+** touches the holder contact marked **+**. This cell only keeps the satellite almanac. It does not run the receiver.
- [ ] Wire only the rows in the table that name a destination. **3.3V** and **VBAT** get no wire.
- [ ] USB in. Set the board by a window. The fix LED blinking at all means VIN and GND are right. A fast blink means it is still searching. A blink about every 15 seconds means it has a fix. No view of the sky means no fix, which is not a wiring fault.

Unplug USB.

## 10. Compass

The silkscreen must say **LSM303AGR**. A blue board labeled **LSM303DLHC** is a different chip. Leave that one in the bag.

This part reports which way is down, and which way is north while the board is held roughly flat. It does not correct the heading when the board is tipped. Later firmware will ask you to hold it level, and will hide the heading when you do not. This checklist does not ask the chip for either reading. It talks on the two wires labeled **SDA** and **SCL**. Those pins are no longer the beep or the GPS enable.

Seat it at the opposite end of the breadboard from the vibration motor before any of its wires go in. The motor is a permanent magnet. Powering the motor off does not remove that magnet. Distance does. The radio antenna is the other end to stay away from.

**VIN** takes power. The compass pin labeled **3V**, the one beside **VIN**, is an output from the breakout's regulator. It is not the Feather's **3V** pin. Leave it empty.

**CSA** and **CSM** choose how the chip talks. Leave them empty. The chip holds them high, which keeps both sensors on **SDA** and **SCL**. A wire from either pin to the blue rail takes that sensor off the bus.

**IA1**, **IA2**, **INTM**, and any other unlisted pin get no wire. The compass does not connect to **D12**.

| Compass pin | Connect to |
|---|---|
| VIN | red rail |
| GND | blue rail |
| SDA | Feather **SDA** |
| SCL | Feather **SCL** |
| 3V (beside VIN) | no wire |
| IA1 | no wire |
| IA2 | no wire |
| INTM | no wire |
| CSA | no wire |
| CSM | no wire |

- [ ] Wire the table.
- [ ] USB in. Measure compass **VIN** against **GND**. About 3.3 volts. The heading will not appear. Nothing in this checklist asks the chip for one.

Unplug USB.

## 11. Battery, last

- [ ] USB stays unplugged.
- [ ] Use the white two-pin socket soldered on the Feather. The socket on the radio stays empty.
- [ ] The Adafruit plug slides into the Feather socket without force. If you have to force it, stop. You have the radio's battery.
- [ ] Black probe on the blue rail, red probe on the red rail. About 3.3 volts, now from the battery through the Feather's regulator.
- [ ] Then plug USB in, with the pack still in the LiPo bag, and stay in the room. The Feather charges while it runs.

## Stop checks

Unplug the LiPo. Plug in only USB. These boxes stay empty unless the bad condition is happening. If one becomes true, unplug USB before you move a wire.

- [ ] The continuity meter beeps with one probe in the red rail and the other in the blue rail.
- [ ] The red rail is above 3.6 volts or below 3.0 volts with USB connected.
- [ ] Any module is too hot to touch after a few seconds.
- [ ] The screen, radio, GPS, or compass has a wire on the Feather pin labeled **USB** or **BAT**.
- [ ] The radio's **3V3_EN**, **VBUS**, or **VSYS** socket has a wire.
- [ ] The GPS pin labeled **3.3V**, or the compass pin labeled **3V** beside **VIN**, has a wire.
- [ ] Compass **CSA** or **CSM** is tied to the blue rail.
- [ ] Any compass pin other than **SDA** and **SCL** has a wire to a Feather signal pin. **D12** is the radio only.

## What "done" means

- [ ] Feather alone shows `FTHR840BOOT` after a double-tap of reset.
- [ ] About 3.3 volts on the red rail, on the radio's **3V3** socket, and on screen pin 7.
- [ ] Radio PWR LED lit. Radio CHG LED dark.
- [ ] Antenna fitted. `GP22` still empty.
- [ ] Screen glass lit, picture still blank or speckled.
- [ ] Back button reads about 3.3 volts and falls to 0 while pressed.
- [ ] GPS LED blinking when the antenna can see sky.
- [ ] Motor still, piezo silent.
- [ ] Compass powered from the red rail, **SDA** and **SCL** on those Feather pins, **CSA** and **CSM** empty, **D12** connected only to radio `GP15`, **PPS** still empty.
- [ ] Adafruit LiPo in the Feather's white socket. Radio battery still in the bag.

The pager application is a separate piece of work. `hal/nrf/nrf_hal.c` is still a stub. Until that stub drives these exact pins, the screen will not show a menu and the radio will not send a page.

## Related

- [hardware.md](hardware.md) explains why the pager uses this class of part.
- [hal.md](hal.md) is the function list the firmware will call once the pins above are real.
