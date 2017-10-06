#include <Simple-INA226.h>

SIMPLE_INA226 ina;

void setup() {
  Serial.begin(115200);
  delay(1000);
  ina.init(0.1, 150);
}

void loop() {

  Serial.print(" Volt: ");
  Serial.print(ina.getVoltage());
  Serial.print(" V, Current: ");
  Serial.print(ina.getCurrent()*1000);
  Serial.print(" mA, Power: ");
  Serial.print(ina.getPower()*1000);
  Serial.print(" mW");
  Serial.println();
  yield;
  delay(1000);
}
