// This example shows how to read temperature/pressure

#include <Adafruit_DPS310.h>

Adafruit_DPS310 dps;

// Can also use SPI!
#define DPS310_CS 10

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("DPS310");
  if (! dps.begin_I2C()) {             // Can pass in I2C address here
  //if (! dps.begin_SPI(DPS310_CS)) {  // If you want to use SPI
    Serial.println("Failed to find DPS");
    while (1) yield();
  }
  Serial.println("DPS OK!");

  dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
  dps.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);
}

void loop() {
  sensors_event_t temp_event, pressure_event;
  
  while (!dps.temperatureAvailable() || !dps.pressureAvailable()) {
    return; // wait until there's something to read
  }

  dps.getEvents(&temp_event, &pressure_event);
  Serial.print(F("Temperature = "));
  Serial.print(temp_event.temperature);
  Serial.println(" *C");

  Serial.print(F("Pressure = "));
  Serial.print(pressure_event.pressure);
  Serial.println(" hPa"); 

  Serial.println();
}
