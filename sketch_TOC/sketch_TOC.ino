#include <Arduino_Modulino.h>

ModulinoDistance distanceSensor;
ModulinoPixels pixels;

const int PIXEL_COUNT = 8;
const int PIXEL_BRIGHTNESS = 25;

enum LightState { LIGHT_RED, LIGHT_GREEN, LIGHT_YELLOW };
LightState currentState = LIGHT_RED;
unsigned long stateChangedAt = 0;

unsigned long stateDurationMs(LightState state) {
  switch (state) {
    case LIGHT_RED:    return 6000; // 6 seconds of red
    case LIGHT_GREEN:  return 5000; // 5 seconds of green
    case LIGHT_YELLOW: return 2000; // 2 seconds of yellow/amber
  }
  return 4000;
}

void showTrafficLight(LightState state) {
  int color = RED;
  switch (state) {
    case LIGHT_GREEN:  color = GREEN; break;
    case LIGHT_YELLOW: color = WHITE; break; // Modulino Pixels lack yellow; white ≈ amber
    default:           color = RED; break;
  }

  for (int i = 0; i < PIXEL_COUNT; i++) {
    pixels.set(i, color, PIXEL_BRIGHTNESS);
  }
  pixels.show();
}

void advanceTrafficLight() {
  if (currentState == LIGHT_RED) {
    currentState = LIGHT_GREEN;
  } else if (currentState == LIGHT_GREEN) {
    currentState = LIGHT_YELLOW;
  } else {
    currentState = LIGHT_RED;
  }
  stateChangedAt = millis();
  showTrafficLight(currentState);
}

void updateTrafficLight(unsigned long now) {
  if (now - stateChangedAt >= stateDurationMs(currentState)) {
    advanceTrafficLight();
  }
}

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
  Serial.println("Modulino Distance + Pixels traffic light test");
  Serial.println("-------------------------------------------------");

  Modulino.begin();

  if (!distanceSensor.begin()) {
    stopWithError("Distance tile not detected. Check Qwiic connections.");
  }
  if (!pixels.begin()) {
    stopWithError("Pixels tile not detected. Check Qwiic connections.");
  }

  stateChangedAt = millis();
  showTrafficLight(currentState);
  Serial.println("Tiles initialized. Pixels are cycling Red → Green → Yellow.");
  Serial.println("Distance readings continue below.");
}

void serviceDistanceSensor() {
  if (!distanceSensor.available()) {
    return;
  }

  int measure = distanceSensor.get();
  Serial.print("Distance: ");
  Serial.print(measure);
  Serial.println(" mm");
}

void loop() {
  unsigned long now = millis();
  updateTrafficLight(now);
  serviceDistanceSensor();
  delay(20);
}
