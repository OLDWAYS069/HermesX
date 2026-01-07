// Demo for getting individual unified sensor data from the LPS2X series

#include <Adafruit_LPS2X.h>

// Use LPS25 or LPS22 here
Adafruit_LPS25 lps;
Adafruit_Sensor *lps_temp, *lps_pressure;

void setup(void) {
  Serial.begin(115200);
  while (!Serial)
    delay(10); // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit LPS2X test!");

  if (!lps.begin_I2C()) {
    Serial.println("Failed to find LPS2X chip");
    while (1) {
      delay(10);
    }
  }

  Serial.println("LPS2X Found!");
  lps_temp = lps.getTemperatureSensor();
  lps_temp->printSensorDetails();

  lps_pressure = lps.getPressureSensor();
  lps_pressure->printSensorDetails();
}

void loop() {
  //  /* Get a new normalized sensor event */
  sensors_event_t pressure;
  sensors_event_t temp;
  lps_temp->getEvent(&temp);
  lps_pressure->getEvent(&pressure);

  Serial.print("\t\tTemperature ");
  Serial.print(temp.temperature);
  Serial.println(" deg C");

  /* Display the results (pressure is measured in hPa) */
  Serial.print("\t\tPressure: ");Serial.print(pressure.pressure);Serial.println(" hPa");
  Serial.print("\t\tTemperature: ");Serial.print(temp.temperature);Serial.println(" degrees C");


  delay(100);

  /*//   serial plotter friendly format
  Serial.print(temp.temperature);
  Serial.print(",");

  Serial.print(pressure.pressure);

  Serial.println();
  delay(10);
  */
}
