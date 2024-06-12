
This prototype runs on a Teensy 4.1 connected to the Pizza Primavera Ventiquattro interface board

![8659163656192-Produce_DanZhi SMT_Snapshot Top 3614621A_Y43 SMT02403151649857](https://github.com/datomusic/drum-firmware/assets/869645/5d7b32ab-f92d-4832-bbf6-9fe57343b2e0)

Pinout for the 40 pin connector is as follows:
| Pin | Name | Function | Teensy 4.1 pin |
| ---: | --- | --- | --- |
| 1 | GNDD | Digital Ground for the LEDs | GND |
| 2 | LED_DATA | Data input for the addressable LEDs | D2 |
| 3 | VLED | Positive voltage for the LEDs | 5V |
| 4 | ROW1 |  Innermost ring of buttons | D3 |
| 5 | ROW2 | Inner middle ring of buttons | D4 |
| 6 | ROW3 | Outer middle ring of buttons | D5 |
| 7 | ROW4 | Outermost ring of buttons | D6 |
| 8 | ROW5 | Sample select switches | D9 |
| 9 | COL1 | | D10 |
| 10 | COL2 | | D11 |
| 11 | COL3 | | D12 |
| 12 | COL4 | | D13 |
| 13 | COL5 | | D14 |
| 14 | COL6 | | D15 |
| 15 | COL7 | | D16 |
| 16 | COL8 | | D17 |
| 17 | PLAYBUTTON | Play button signal. A 10kΩ pullup to V+ is present | D37 |
| 18 | GND | Signal ground. Connected to analog ground on the board | GND |
| 19 | V+ | Positive voltage for the pots and FSRs | 3V |
| 20 | FX1 | | A0 |
| 21 | PITCH1 | POT | A1 |
| 22 | DRUM1 | FSR | A2 |
| 23 | MUTE1 | FSR | A3 |
| 24 | VOLUME | POT | A4 |
| 25 | FX2R | FSR | D8 |
| 26 | FX2L | FSR | D7 | 
| 27 | PITCH2 | FSR | A5 |
| 28 | DRUM2 | FSR | A6 |
| 29 | MUTE2 | FSR | A7 |
| 30 | FX3 | FSR | A8 |
| 31 | PITCH3 | POT | A9 |
| 32 | DRUM3 | FSR | A10 |
| 33 | MUTE3 | FSR | A11 |
| 34 | TEMPO | Pot | A12 |
| 35 | FX4R | FSR | A13 |
| 36 | FX4L | FSR | A14 (D38) |
| 37 | PITCH4 | POT | A15 (D39) |
| 38 | DRUM4 | FSR | A16 (D40) |
| 39 | MUTE4 | FSR | A17 (D41) |
| 40 | GNDA | Analog ground. Connected to signal ground on the board | GND |