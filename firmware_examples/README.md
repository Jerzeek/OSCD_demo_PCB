# Firmware examples

Arduino sketches for the OSCD Demo PCB rev 1.0.

| Sketch | What it does |
|---|---|
| [`test_sensors/`](test_sensors) | Exercises every subsystem on the board — LEDs, IMU, light sensor, touch pads and buttons. Use it as a bring-up test and as a starting point for your own code. |

## Toolchain setup

1. Install the **Arduino IDE**.
2. **Boards Manager** → install **esp32** by Espressif Systems.
3. **Tools → Board** → *ESP32C3 Dev Module*.
4. **Tools → USB CDC On Boot** → **Enabled**. This is required. The board has no USB-UART bridge, so `Serial` runs over the ESP32-C3's native USB peripheral and will not appear at all if CDC is left disabled.
5. **Library Manager** → install:
   - `Adafruit NeoPixel`
   - `Adafruit LSM6DS` (pulls in `Adafruit_LSM6DS3TRC`)
   - `Adafruit Unified Sensor`
   - `Adafruit MPR121`

The APDS-9306-065 light sensor has no widely available Arduino library, so `test_sensors` talks to it directly over `Wire`. Those helpers live in [`test_sensors/APDS9306.h`](test_sensors/APDS9306.h) and double as a compact example of raw I²C register access.

## Uploading

There is no auto-reset circuit on this board, so the bootloader is entered by hand:

1. Close every open Serial Monitor — an open port blocks the upload.
2. **Hold BOOT (`SW2`).**
3. **Tap RESET (`SW1`).**
4. **Release BOOT.**
5. Click **Upload** in the IDE.
6. When the upload finishes, **tap RESET** to start the new firmware.

If the port does not appear, repeat steps 2–4; in bootloader mode the board enumerates under a different name than when running your sketch.

## What `test_sensors` does

On boot the ring plays a two-turn rainbow sweep, then the sketch runs continuously:

| Input | Effect |
|---|---|
| Buttons `SW3`/`SW4`/`SW5` | Each lights one third of the ring (4 LEDs) in its own hue while held |
| Tilt (LSM6DS3) | A red dot tracks the direction of gravity around the ring; brightness follows tilt magnitude |
| Ambient light (APDS-9306) | Scales overall ring brightness, so the board dims in a dark room |
| Touch pads (MPR121) | Each of the 12 electrodes lights its matching LED, brightness following touch strength |

Every 500 ms the sketch prints IMU, light and touch readings to **Serial at 115200 baud**. Each subsystem is probed at startup and reported as `[OK]` or `[FAIL]`, and the main loop skips whatever did not answer — so a single unpopulated or badly soldered sensor will not stop the rest of the board from working. That makes the sketch a useful first test on a freshly assembled board.

## Pin reference

```
I2C SDA        GPIO7    LSM6DS3 @0x6A, APDS-9306-065 @0x52, MPR121 @0x5A
I2C SCL        GPIO6
WS2812 data    GPIO8    12 LEDs, D1 at 9 o'clock, chain runs clockwise
Button SW3     GPIO1    active low
Button SW4     GPIO0    active low
Button SW5     GPIO2    active low
MPR121 IRQ     GPIO10   unused by this sketch
LSM6DS3 INT1   GPIO4    unused by this sketch
LSM6DS3 INT2   GPIO5    unused by this sketch
```

Buttons have external pull-ups and RC debouncing on the board, so `pinMode(pin, INPUT)` is correct — do not enable `INPUT_PULLUP`.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| No serial port at all | *USB CDC On Boot* is disabled, or a data-less charge-only USB-C cable |
| Upload fails / port vanishes | Serial Monitor still open, or the BOOT/RESET sequence was not held long enough |
| `[FAIL]` on one sensor | Solder bridge or open joint on that IC — check continuity on SDA/SCL at `TP6`/`TP5` |
| All I²C devices fail | `R15`/`R16` pull-ups missing, or `U4` not delivering 3.3 V (check `TP4`) |
| LEDs stay dark | Data path `GPIO8 → R22 → D1.DIN`, or `D16`/`D17`/`D18` not fitted |
| LEDs flicker or the board browns out | Ring brightness too high for a default-current USB port — lower `strip.setBrightness()` |

## Licence

CERN-OHL-P v2, same as the hardware. See [`LICENSE`](../LICENSE).
