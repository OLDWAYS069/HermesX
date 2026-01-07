#include <Adafruit_LPS2X.h>

/**
 * @brief Destroy the Adafruit_LPS25::Adafruit_LPS25 object
 *
 */
Adafruit_LPS25::~Adafruit_LPS25(void) {
  if (temp_sensor)
    delete temp_sensor;
  if (pressure_sensor)
    delete pressure_sensor;
}

/*!  @brief Initializer for post i2c/spi init
 *   @param sensor_id Optional unique ID for the sensor set
 *   @returns True if chip identified and initialized
 */
bool Adafruit_LPS25::_init(int32_t sensor_id) {

  Adafruit_BusIO_Register chip_id = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS2X_WHOAMI, 1);

  // make sure we're talking to the right chip
  uint8_t id = chip_id.read();

  if (id != LPS25HB_CHIP_ID) {
    return false;
  }
  _sensorid_pressure = sensor_id;
  _sensorid_temp = sensor_id + 1;

  temp_scaling = 480;
  temp_offset = 42.5;
  inc_spi_flag = 0x40;

  ctrl1_reg = new Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS25_CTRL_REG1, 1);
  ctrl2_reg = new Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS25_CTRL_REG2, 1);
  ctrl3_reg = new Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS25_CTRL_REG3, 1);
  threshp_reg = new Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS25_THS_P_L_REG, 1);

  reset();
  // do any software reset or other initial setup
  powerDown(false);
  setDataRate(LPS25_RATE_25_HZ);

  pressure_sensor = new Adafruit_LPS2X_Pressure(this);
  temp_sensor = new Adafruit_LPS2X_Temp(this);
  delay(10); // delay for first reading
  return true;
}

/**
 * @brief Sets the rate at which pressure and temperature measurements
 *
 * @param new_data_rate The data rate to set. Must be a `lps25_rate_t`
 */
void Adafruit_LPS25::setDataRate(lps25_rate_t new_data_rate) {
  Adafruit_BusIO_Register ctrl1 = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS25_CTRL_REG1, 1);
  Adafruit_BusIO_RegisterBits data_rate =
      Adafruit_BusIO_RegisterBits(&ctrl1, 3, 4);

  data_rate.write((uint8_t)new_data_rate);

  isOneShot = (new_data_rate == LPS25_RATE_ONE_SHOT) ? true : false;
}

/**
 * @brief Gets the current rate at which pressure and temperature measurements
 * are taken
 *
 * @return lps25_rate_t The current data rate
 */
lps25_rate_t Adafruit_LPS25::getDataRate(void) {
  Adafruit_BusIO_Register ctrl1 = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, LPS25_CTRL_REG1, 1);
  Adafruit_BusIO_RegisterBits data_rate =
      Adafruit_BusIO_RegisterBits(&ctrl1, 3, 4);

  return (lps25_rate_t)data_rate.read();
}

/**
 * @brief Puts the sensor into power down mode, shutting the sensor down
 *
 * @param power_down
 */
void Adafruit_LPS25::powerDown(bool power_down) {
  Adafruit_BusIO_RegisterBits pd = Adafruit_BusIO_RegisterBits(ctrl1_reg, 1, 7);
  pd.write(!power_down); // pd bit->0 == power down
}

/**
 * @brief Configures the INT pin, by default it will output DRDY signal
 * @param activelow Pass true to make the INT pin drop low on interrupt
 * @param opendrain Pass true to make the INT pin an open drain output
 * @param pres_high If true, interrupt fires on high threshold pass
 * @param pres_low If true, interrupt fires on low threshold pass
 */
void Adafruit_LPS25::configureInterrupt(bool activelow, bool opendrain,
                                        bool pres_high, bool pres_low) {
  uint8_t reg =
      (activelow << 7) | (opendrain << 6) | (pres_low << 1) | pres_high;
  ctrl3_reg->write(reg);
}
