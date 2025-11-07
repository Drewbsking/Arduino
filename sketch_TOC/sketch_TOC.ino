#include <Arduino_Modulino.h>

ModulinoDistance distanceSensor;

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
  Serial.println("Modulino Distance quick test");
  Serial.println("--------------------------------");

  Modulino.begin();

  if (!distanceSensor.begin()) {
    stopWithError("Distance tile not detected. Check Qwiic connections.");
  }

  Serial.println("Distance tile initialized. Move an object in front of it.");
}

void loop() {
  if (!distanceSensor.available()) {
    delay(5);
    return;
  }

  int measure = distanceSensor.get();
  Serial.print("Distance: ");
  Serial.print(measure);
  Serial.println(" mm");
  delay(50);
}
