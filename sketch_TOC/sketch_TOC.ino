#include <Wire.h>
#include <Arduino_Modulino.h>

VL53L4CD distanceSensor(&Wire, -1);
VL53L4CD_Result_t lastReading;

void stopWithError(const char* message) {
  Serial.println(message);
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.println("UNO R4 + Modulino Distance sanity check");
  Serial.println("---------------------------------------");

  Wire.begin();

  if (distanceSensor.begin() != 0) {
    stopWithError("Sensor not detected. Check the Qwiic cable and power.");
  }

  if (distanceSensor.InitSensor() != VL53L4CD_ERROR_NONE) {
    stopWithError("Sensor failed to initialize.");
  }

  distanceSensor.VL53L4CD_SetRangeTiming(33, 0);
  distanceSensor.VL53L4CD_StartRanging();

  Serial.println("Sensor ready. Hold something in front of the Modulino Distance tile.");
}

void loop() {
  uint8_t ready = 0;
  distanceSensor.VL53L4CD_CheckForDataReady(&ready);
  if (!ready) {
    delay(5);
    return;
  }

  distanceSensor.VL53L4CD_ClearInterrupt();

  if (distanceSensor.VL53L4CD_GetResult(&lastReading) != VL53L4CD_ERROR_NONE) {
    Serial.println("Couldn't read distance. Waiting...");
    delay(100);
    return;
  }

  if (lastReading.range_status == 0) {
    Serial.print("Distance: ");
    Serial.print(lastReading.distance_mm);
    Serial.println(" mm");
  } else {
    Serial.print("Out of range (status ");
    Serial.print(lastReading.range_status);
    Serial.println(")");
  }

  delay(100);
}
