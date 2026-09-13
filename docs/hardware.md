# Hardware design notes

Reference for the OSCD Demo PCB rev 1.0. Designators match [`schematic.pdf`](../schematic.pdf) and the interactive BOM in [`bom/ibom.html`](../bom/ibom.html).

## Power

```
USB-C VBUS ──┬── U1 USBLC6-2SC6 (ESD)
             ├── TP3 (via R29 100R)
             ├── J2.1 / J3.1  (+5V to headers)
             ├── D16 / D17 / D18  1N5819WS ──► WS2812B VDD (3 groups of 4)
             └── U4 HT7533-1 (SOT-89 LDO) ──► +3V3 ──┬── U2 ESP32-C3-MINI-1
                                                     ├── U3 / U5 / U6 sensors
                                                     └── TP4 (via R30 100R)
```

The board is bus-powered from USB-C only; there is no battery input or charger.

**Why the Schottky diodes?** The WS2812B-2020 LEDs are fed from 5 V through `D16`/`D17`/`D18` rather than directly. Each diode drops roughly 0.3 V, putting LED V<sub>DD</sub> near 4.7 V. That brings the WS2812B logic-high threshold (~0.7 × V<sub>DD</sub>) down far enough that the ESP32-C3's 3.3 V data output is reliably recognised, without a level shifter. The 12 LEDs are split across three diodes so no single diode carries the full ring current.

`C12`–`C16` (10 µF) and `C17`/`C18` (100 nF) provide bulk and local decoupling; every IC has its own 100 nF.

## USB

`J1` is a 16-pin GCT USB4105 USB-C receptacle. `R4`/`R5` (5.1 kΩ) are the CC pull-downs that advertise the board as a UFP drawing default current. `R11` (1 MΩ) ties the shell to ground for HF bonding. `C11` (4.7 nF / 1 kV) is the shield-to-ground safety cap.

D+/D− pass through `U1` (USBLC6-2SC6 ESD array) and then `R9`/`R10` (0 Ω, fitted) to the ESP32-C3's `GPIO19`/`GPIO18`. The 0 Ω links exist so the USB pair can be cut and re-routed during bring-up.

There is **no USB-UART bridge and no auto-reset transistor pair.** Uploads go over the ESP32-C3's built-in USB serial/JTAG peripheral, and the bootloader must be entered manually with the BOOT and RESET buttons.

## Microcontroller

`U2` is an ESP32-C3-MINI-1 module (integrated antenna, 4 MB flash). Strapping/reset support:

| Part | Net | Purpose |
|---|---|---|
| `R1` 10 kΩ | `EN` → 3V3 | Reset pull-up; `SW1` pulls `EN` low |
| `R12` 10 kΩ | `GPIO9` → 3V3 | BOOT strapping pull-up; `SW2` pulls it low |
| `R2` 10 kΩ | `GPIO2` → 3V3 | Strapping pin must be high at boot |
| `R3` 10 kΩ | `GPIO8` → 3V3 | Strapping pin must be high at boot |

Because `GPIO2` and `GPIO8` are strapping pins that carry the pull-ups above, they are safe to use as inputs/outputs after boot but must not be held low at reset — `GPIO8` in particular is the LED data line, so keep the ring quiet until firmware starts.

## User buttons

| Button | Net | Pull-up | Debounce |
|---|---|---|---|
| `SW3` | `GPIO1` via `R34` (0 Ω) | `R6` 10 kΩ | `C6` 100 nF |
| `SW4` | `GPIO0` via `R33` (0 Ω) | `R7` 10 kΩ | `C7` 100 nF |
| `SW5` | `GPIO2` via `R31` (0 Ω) | **`R8` is DNP** | `C8` 100 nF |

All three are active low, so firmware uses plain `INPUT` (no internal pull-up needed). `R8` is deliberately not fitted: `GPIO2` already carries the strapping pull-up `R2`, and a second 10 kΩ in parallel would only halve the pull-up value.

`SW1` (RESET) and `SW2` (BOOT) are the small SMD tactile switches `TS-1088-AR02016`; the three user buttons are the larger through-hole-style `PTS645`.

## I²C bus

One 400 kHz bus on `GPIO7` (SDA) and `GPIO6` (SCL), pulled up by `R15`/`R16` (4.7 kΩ) and brought out to `TP6`/`TP5` through 100 Ω series resistors for safe scope probing.

| Device | Address | Interrupt |
|---|---|---|
| `U3` LSM6DS3 IMU | `0x6A` | `INT1` → `GPIO4`, `INT2` → `GPIO5` (via 0 Ω `R24`/`R23`) |
| `U5` APDS-9306-065 light sensor | `0x52` | `INT` → `GPIO3` (via 0 Ω `R25`) |
| `U6` MPR121 touch controller | `0x5A` | `IRQ` → `GPIO10` (via 0 Ω `R32`) |

The interrupt lines are wired but unused by the demo firmware — they are there for anyone who wants to move from polling to interrupt-driven reads.

### Address-select options

Both configurable devices are set by 0 Ω links, so the addresses can be changed with a soldering iron rather than a re-spin.

**MPR121 (`U6`) `ADDR` pin** — fit exactly one:

| Link | `ADDR` tied to | Resulting address |
|---|---|---|
| `R21` ✅ *fitted* | GND | `0x5A` |
| `R18` (+ `R28` 10 kΩ) | 3V3 | `0x5B` |
| `R20` | SCL | `0x5C` |
| `R19` | SDA | `0x5D` |

**LSM6DS3 (`U3`) `SDO/SA0` pin** — fit exactly one:

| Link | `SA0` tied to | Resulting address |
|---|---|---|
| `R27` ✅ *fitted* | GND | `0x6A` |
| `R26` (+ `R14` 10 kΩ) | 3V3 | `0x6B` |

`R13` (10 kΩ) holds the IMU's `CS` high, selecting I²C mode. `U3`'s `SDX`/`SCX` are grounded (no auxiliary sensor hub device).

## LED ring

Twelve WS2812B-2020 LEDs on a 63 mm-diameter circle, 30° apart, `D1` at the 9 o'clock position with the data chain running clockwise `D1 → D12`. `D12`'s `DOUT` is unterminated. Data enters `D1` through `R22` (0 Ω) from `GPIO8`.

At full white and full brightness twelve WS2812Bs draw well over half an amp, which a USB-C default-current port may not like. The demo firmware caps brightness (`strip.setBrightness()`), and you should too.

## Capacitive touch

`U6` (MPR121) drives twelve electrodes etched directly into the top copper as custom footprints:

| Footprint | Designator | Electrodes | Shape |
|---|---|---|---|
| `6_pad_touch_slider` | `TP2` | `ELE0`–`ELE5` | Inner 6-segment slider |
| `6_pad_touch_buttons` | `TP1` | `ELE6`–`ELE11` | Outer 6 discrete buttons |

`R17` (75 kΩ, 1 %) is the `REXT` reference resistor that sets the MPR121's charge current — do not substitute a loose-tolerance part. The electrode nets are assigned to the `CAP` net class (0.1 mm tracks) to keep parasitic capacitance low and consistent.

## Test points

| TP | Signal | Series resistor |
|---|---|---|
| `TP3` | +5 V | `R29` 100 Ω |
| `TP4` | +3.3 V | `R30` 100 Ω |
| `TP5` | SCL | `R35` 100 Ω |
| `TP6` | SDA | `R36` 100 Ω |

The 100 Ω resistors limit fault current if a probe slips, so a short at a test point cannot damage the bus or the regulator.

## Board construction

- **4 layers:** `F.Cu` (signal) / `In1.Cu` (power) / `In2.Cu` (power) / `B.Cu` (signal)
- **Stackup:** 0.035 / 0.1 prepreg / 0.035 / 1.24 core / 0.035 / 0.1 prepreg / 0.035 mm → **1.6 mm** finished
- **Outline:** the OSCD logo silhouette, roughly 93 × 93 mm
- **Assembly:** single-sided — every component is on the top layer

**Net classes**

| Class | Track width | Via | Applied to |
|---|---|---|---|
| `Default` | 0.15 mm | 0.45 / 0.3 mm | everything else |
| `PWR` | 0.40 mm | 0.60 / 0.3 mm | `+5V`, `+3.3V`, `GND`, LED V<sub>DD</sub> nets |
| `CAP` | 0.10 mm | — | MPR121 electrode nets |

## Known quirks in rev 1.0

- `J2` and `J3` carry the Value field `Conn_01x12` while the symbol and footprint are both 2×5 (10-pin). The netlist and layout are correct; only the displayed value string is wrong, and it propagates into `production/bom.csv`. Worth fixing before a rev 1.1.
- `R8` is DNP by design (see [User buttons](#user-buttons)) and so does not appear in `production/bom.csv`.
- `R14`, `R28` and `R13` are fitted, but `R14`/`R28` only do anything if the matching address-select link (`R26`/`R18`) is also fitted.
