// ---------- APDS-9306-065 (direct I2C, no library) ----------
#define APDS9306_MAIN_CTRL 0x00
#define APDS9306_ALS_MEAS_RATE 0x04
#define APDS9306_ALS_GAIN 0x05
#define APDS9306_PART_ID 0x06
#define APDS9306_MAIN_STATUS 0x07
#define APDS9306_ALS_DATA_0 0x0D  // 3 bytes, LSB first

// ======================================================================
//  APDS-9306-065 helpers
// ======================================================================
void apds9306WriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS9306_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t apds9306ReadReg(uint8_t reg) {
  Wire.beginTransmission(APDS9306_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(APDS9306_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

bool apds9306Init() {
  uint8_t id = apds9306ReadReg(APDS9306_PART_ID);
  if (id != 0xB1 && id != 0xB3) return false;  // 0xB3 = APDS-9306-065

  apds9306WriteReg(APDS9306_ALS_MEAS_RATE, 0x22);  // 18-bit resolution, 100 ms rate
  apds9306WriteReg(APDS9306_ALS_GAIN, 0x00);       // 1x gain
  apds9306WriteReg(APDS9306_MAIN_CTRL, 0x02);      // ALS_EN
  return true;
}

// Returns raw ALS counts (roughly proportional to lux at default gain/res)
uint32_t apds9306ReadALS() {
  Wire.beginTransmission(APDS9306_ADDR);
  Wire.write(APDS9306_ALS_DATA_0);
  Wire.endTransmission(false);
  Wire.requestFrom(APDS9306_ADDR, (uint8_t)3);
  uint32_t data0 = Wire.available() ? Wire.read() : 0;
  uint32_t data1 = Wire.available() ? Wire.read() : 0;
  uint32_t data2 = Wire.available() ? Wire.read() : 0;
  return data0 | (data1 << 8) | (data2 << 16);
}