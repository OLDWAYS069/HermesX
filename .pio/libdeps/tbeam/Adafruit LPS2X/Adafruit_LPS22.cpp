#include <Adafruit_LPS2X.h>

/**
 * @brief Destroy the Adafruit_LPS22::Adafruit_LPS22 object
 *
 */
Adafruit_LPS22::~Adafruit_LPS22(void) {
  if (temp_sensor)
    delete temp_sensor;
  if (pressure_sensor)
    delete pressure_sensor;
}

/*!  @brief Initializer for post i2c/spi init
 *   @param sensor_id Optional unique ID for the sensor set
 *   @returns True if chip identified and initialized
 */
bool Adafruit_LPS22::_init(int32_t sensor_id) {

  Adafruit_BusIO_Register chip_id = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS2X_WHOAMI, 1);

  // make sure we're talking to the right chip
  uint8_t id = chip_id.read();

  if (id != LPS22HB_CHIP_ID) {
    return false;
  }
  _sensorid_pressure = sensor_id;
  _sensorid_temp = sensor_id + 1;

  temp_scaling = 100;
  temp_offset = 0;

  ctrl1_reg = new Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS22_CTRL_REG1, 1);
  ctrl2_reg = new Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS22_CTRL_REG2, 1);
  ctrl3_reg = new Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS22_CTRL_REG3, 1);
  threshp_reg = new Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS22_THS_P_L_REG, 1);

  reset();
  // do any software reset or other initial setup
  setDataRate(LPS22_RATE_25_HZ);
  // interrupt on data ready
  configureInterrupt(true, false, true);

  pressure_sensor = new Adafruit_LPS2X_Pressure(this);
  temp_sensor = new Adafruit_LPS2X_Temp(this);

  delay(10); // delay for first reading
  return true;
}

/**
 * @brief Sets the rate at which pressure and temperature measurements
 *
 * @param new_data_rate The data rate to set. Must be a `lps22_rate_t`
 */
void Adafruit_LPS22::setDataRate(lps22_rate_t new_data_rate) {
  Adafruit_BusIO_RegisterBits data_rate =
      Adafruit_BusIO_RegisterBits(ctrl1_reg, 3, 4);

  data_rate.write((uint8_t)new_data_rate);

  isOneShot = (new_data_rate == LPS22_RATE_ONE_SHOT) ? true : false;
}

/**
 * @brief Gets the current rate at which pressure and temperature measurements
 * are taken
 *
 * @return lps22_rate_t The current data rate
 */
lps22_rate_t Adafruit_LPS22::getDataRate(void) {
  Adafruit_BusIO_RegisterBits data_rate =
      Adafruit_BusIO_RegisterBits(ctrl1_reg, 3, 4);

  return (lps22_rate_t)data_rate.read();
}

/**
 * @brief Configures the INT pin
 * @param activelow Pass true to make the INT pin drop low on interrupt
 * @param opendrain Pass true to make the INT pin an open drain output
 * @param data_ready If true, interrupt fires on new data ready
 * @param pres_high If true, interrupt fires on high threshold pass
 * @param pres_low If true, interrupt fires on low threshold pass
 * @param fifo_full If true, interrupt fires on fifo full
 * @param fifo_watermark If true, interrupt fires on fifo watermark pass
 * @param fifo_overflow If true, interrupt fires on fifo overflow
 */
void Adafruit_LPS22::configureInterrupt(bool activelow, bool opendrain,
                                        bool data_ready, bool pres_high,
                                        bool pres_low, bool fifo_full,
                                        bool fifo_watermark,
                                        bool fifo_overflow) {
  uint8_t reg = (activelow << 7) | (opendrain << 6) | (fifo_full << 5) |
                (fifo_watermark << 4) | (fifo_overflow << 3) |
                (data_ready << 2) | (pres_low << 1) | (pres_high);
  ctrl3_reg->write(reg);
}
