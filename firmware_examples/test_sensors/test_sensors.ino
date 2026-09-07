/*  OSCD Demo PCB — Firmware demo
 *  Open Science Community Delft
 *  
 *  v1.0, 07-09-2026, Dr.ir. Tim M.J. Nijssen
 *  CERN-OHL-P license
 *
 *  Tests all sensors on the board to see that each subsystem works:
 *   - 12x WS2812B RGB LEDs
 *   - LSM6DS3 accelerometer/gyroscope (I2C)
 *   - APDS-9306-065 ambient light sensor (I2C, direct register access)
 *   - MPR121 12-channel capacitive touch controller (I2C)
 *   - 3x push buttons
 *
 *  ---------------------------------------------------------------------
 *  ARDUINO IDE SETUP
 *  ---------------------------------------------------------------------
 *  1. Board package: "esp32" by Espressif Systems (Boards Manager)
 *  2. Board: "ESP32C3 Dev Module"
 *  3. Tools -> USB CDC On Boot: "Enabled"   <-- required, or Serial won't
 *     appear over the native USB port
 *  4. This board has no USB-UART bridge, so there is no auto-reset circuit.
 *     To upload: hold BOOT (SW2), tap RESET (SW1), release BOOT, THEN
 *     click Upload. Ensure all serial monitors are closed before uploading.
 *  5. After uploading is completed, tap RESET (SW1)
 *
 *  LIBRARIES (all installable via Library Manager)
 *   - "Adafruit NeoPixel"            (WS2812B)
 *   - "Adafruit LSM6DS"              (IMU — pulls in Adafruit_LSM6DS3TRC)
 *   - "Adafruit Unified Sensor"      (dependency of the above)
 *   - "Adafruit MPR121"              (capacitive touch)
 *  (The APDS-9306-065 light sensor has no common Arduino library, so it's
 *   talked to directly over Wire — this also doubles as a simple example
 *   of raw I2C register access.)
 *
 *  ---------------------------------------------------------------------
 *  BOARD PINOUT (OSCD Demo PCB Rev 1.0)
 *  ---------------------------------------------------------------------
 *  I2C SDA          GPIO7   (LSM6DS3 @0x6A, APDS-9306-065 @0x52, MPR121 @0x5A)
 *  I2C SCL          GPIO6
 *  WS2812 data      GPIO8   (12 LEDs)
 *  Button SW3       GPIO1   (active low)
 *  Button SW4       GPIO0   (active low)
 *  Button SW5       GPIO2   (active low)
 *  MPR121 IRQ       GPIO10  (not used here)
 *  LSM6DS3 INT1     GPIO4   (not used here)
 *  LSM6DS3 INT2     GPIO5   (not used here)
 */

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_LSM6DS3TRC.h>
#include <Adafruit_MPR121.h>

// ---------- Pin definitions ----------
#define PIN_SDA 7
#define PIN_SCL 6

#define PIN_LED_DATA 8
#define NUM_LEDS 12

#define NUM_BTNS 3
#define PIN_BTN_SW3 1
#define PIN_BTN_SW4 0
#define PIN_BTN_SW5 2

#define NUM_TOUCH 12

#define MPR121_ADDR 0x5A
#define LSM6DS3_ADDR 0x6A
#define APDS9306_ADDR 0x52

// --- APDS-9306-065 helper functions ---
#include <APDS9306.h>

// ---------- Objects ----------
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_DATA, NEO_GRB + NEO_KHZ800);
Adafruit_LSM6DS3TRC imu;
Adafruit_MPR121 touch = Adafruit_MPR121();

bool imuOk = false;
bool touchOk = false;
bool alsOk = false;

uint32_t lastSensorPrint = 0;

// ======================================================================
//  Setup
// ======================================================================
void setup() {
  // --- Serial ---
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {
    delay(10);
  }  // give USB CDC time to enumerate
  delay(100);

  // --- buttons ---
  pinMode(PIN_BTN_SW3, INPUT);  // buttons have external pull-up resistors
  pinMode(PIN_BTN_SW4, INPUT);
  pinMode(PIN_BTN_SW5, INPUT);

  // --- led strip ---
  strip.begin();
  strip.setBrightness(60);
  strip.show();  // all off

  // --- I2C ---
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);

  // --- IMU ---
  if (imu.begin_I2C(LSM6DS3_ADDR, &Wire)) {
    imuOk = true;
  }

  // --- Ambient light sensor ---
  alsOk = apds9306Init();
  Serial.println(alsOk ? "[OK]   APDS-9306-065 light sensor responding"
                       : "[FAIL] APDS-9306-065 light sensor not responding");

  // --- Capacitive touch ---
  if (touch.begin(MPR121_ADDR, &Wire)) {
    touchOk = true;
    touch.setAutoconfig(true);
  }

  // --- startup animation ---
  for (uint16_t i = 0; i < 2 * NUM_LEDS; i++) {
    strip.rainbow(i * 0x10000 / NUM_LEDS, 1, 255, 255, true);
    strip.show();
    delay(100);
  }
  strip.clear();
  strip.show();
}

// ======================================================================
//  Main loop
// ======================================================================
void loop() {
  handleButtons();

  if (alsOk)
    handleAls();

  if (imuOk)
    handleImu();

  if (touchOk)
    handleTouch();

  if (Serial && millis() - lastSensorPrint > 500) {
    lastSensorPrint = millis();
    printSensorReadings();
  }
}

// ---------- Buttons -> LED flashes  ----------
void handleButtons() {
  static bool lastState[NUM_BTNS] = { true, true, true };  // pulled up = true when idle
  int pins[NUM_BTNS] = { PIN_BTN_SW3, PIN_BTN_SW4, PIN_BTN_SW5 };

  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    bool state = digitalRead(pins[i]);
    if (state != lastState[i]) {
      uint8_t first_led = i * NUM_LEDS / NUM_BTNS;       // first led of the segment
      uint8_t last_led = (i + 1) * NUM_LEDS / NUM_BTNS;  // first led of the next segment
      uint16_t hue = i * 0x10000 / NUM_BTNS;

      if (!state) {  // pressed
        for (uint8_t j = first_led; j < last_led; j++) {
          strip.setPixelColor(j, strip.gamma32(strip.ColorHSV(hue, 255, 255)));
        }
      } else {  // released
        for (uint8_t j = first_led; j < last_led; j++) {
          strip.setPixelColor(j, 0);
        }
      }
      strip.show();
    }
    lastState[i] = state;
  }
}

// ---------- Ambient light -> LED brightness ----------
void handleAls() {
  const uint16_t max_lux = 20000;
  uint8_t brightness = map(constrain(apds9306ReadALS(), 0, max_lux), 0, max_lux, 0, 255);
  strip.setBrightness(brightness);
}

// ---------- Orientation -> LED mapping ----------
void handleImu() {
  static uint8_t lastLed = 0;
  const uint16_t hue = 0;

  float accel_x;
  float accel_y;
  float accel_z;

  imu.readAcceleration(accel_x, accel_y, accel_z);
  float angle = atan2(-accel_y, accel_x) + M_PI;
  float magsq = (accel_y*accel_y + accel_x*accel_x);
  
  uint8_t led = ((uint8_t) round(angle/(2*M_PI)*NUM_LEDS)) % NUM_LEDS;
  uint8_t val = (uint8_t) (magsq*255.);

  strip.setPixelColor(led, strip.ColorHSV(hue,255,val));

  if (led != lastLed) {
    strip.setPixelColor(lastLed, 0);
    strip.show();
    lastLed = led;
  }
}

// ---------- Capacitive touch -> LED mapping ----------
void handleTouch() {
  static uint16_t lastTouched = 0;
  const uint8_t led_map[NUM_TOUCH] = { 3, 2, 1, 0, 11, 10, 9, 8, 7, 6, 5, 4 };

  uint16_t currTouched = touch.touched();

  for (int i = 0; i < NUM_TOUCH; i++) {
    bool isTouched = currTouched & (1 << i);  // read bitmask
    bool wasTouched = lastTouched & (1 << i);

    if (isTouched) {                                                       // while pressed
      uint8_t val = (touch.baselineData(i) - touch.filteredData(i)) >> 2;  // touch sensor data, converted from 10 bit to 8 bit
      strip.setPixelColor(led_map[i], strip.ColorHSV(0, 0, val));
      strip.show();
    }

    if (!isTouched && wasTouched) {  // released
      uint8_t led = i % NUM_LEDS;
      strip.setPixelColor(led_map[i], 0);
      strip.show();
    }
  }
  lastTouched = currTouched;
}

// ---------- Periodic IMU / light sensor printout ----------
void printSensorReadings() {
  Serial.print("IMU    ");
  if (imuOk) {
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    imu.getEvent(&accel, &gyro, &temp);

    Serial.print("T[degC]=");
    Serial.print(temp.temperature);

    Serial.print("  accel[m/s^2] X=");
    Serial.print(accel.acceleration.x, 2);
    Serial.print(" Y=:");
    Serial.print(accel.acceleration.y, 2);
    Serial.print(" Z=");
    Serial.print(accel.acceleration.z, 2);

    /* Display the results (rotation is measured in rad/s) */
    Serial.print("  gyro[rad/s] X= ");
    Serial.print(gyro.gyro.x, 1);
    Serial.print(" Y=");
    Serial.print(gyro.gyro.y, 1);
    Serial.print(" Z=");
    Serial.print(gyro.gyro.z, 1);
  } else {
    Serial.print("not detected");
  }
  Serial.println();

  Serial.print("ALS    ");
  if (alsOk) {
    Serial.print("value[counts]=");
    Serial.print(apds9306ReadALS());
    Serial.println(" (roughly proportional to lux)");
  } else {
    Serial.println("not detected");
  }
  Serial.println();

  Serial.print("TOUCH  ");
  if (touchOk) {
    Serial.print("value[counts]");
    for (uint8_t i = 0; i < NUM_TOUCH; i++) {
      Serial.print("pad");
      Serial.print(i);
      Serial.print(" ");
      Serial.print(touch.baselineData(i) - touch.filteredData(i));
      Serial.print(" ");
    }
    Serial.println();
  } else {
    Serial.println("not detected");
  }
}
