#include <Arduino_Modulino.h>

ModulinoDistance distanceSensor;
ModulinoPixels pixels;
ModulinoButtons buttons;

const int PIXEL_COUNT = 9;  // Modulino Pixels is a 3x3 tile
const uint8_t PIXEL_BRIGHTNESS_RED    = 8;
const uint8_t PIXEL_BRIGHTNESS_GREEN  = 8;
const uint8_t PIXEL_BRIGHTNESS_AMBER  = 6;

const uint8_t RED_SEGMENT[]   = {0, 1, 2};      // bottom row
const uint8_t AMBER_SEGMENT[] = {3, 4, 5};      // middle row
const uint8_t GREEN_SEGMENT[] = {6, 7, 8};      // top row

enum LightState { LIGHT_RED, LIGHT_GREEN, LIGHT_YELLOW };
LightState currentState = LIGHT_RED;
unsigned long stateChangedAt = 0;

unsigned long stateDurationMs(LightState state) {
  switch (state) {
    case LIGHT_RED:    return 15000; // 15 seconds of red
    case LIGHT_GREEN:  return 5000; // 5 seconds of green
    case LIGHT_YELLOW: return 2000; // 2 seconds of yellow/amber
  }
  return 4000;
}

template <size_t N>
void paintSegment(const uint8_t (&indices)[N], uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  for (size_t i = 0; i < N; ++i) {
    if (indices[i] >= PIXEL_COUNT) continue;
    pixels.set(indices[i], r, g, b, brightness);
  }
}

void clearPixels() {
  for (int i = 0; i < PIXEL_COUNT; ++i) {
    pixels.set(i, 0, 0, 0, 0);
  }
}

void showTrafficLight(LightState state) {
  clearPixels();

  switch (state) {
    case LIGHT_GREEN:
      paintSegment(GREEN_SEGMENT, 0, 255, 0, PIXEL_BRIGHTNESS_GREEN);
      break;
    case LIGHT_YELLOW:
      paintSegment(AMBER_SEGMENT, 255, 110, 0, PIXEL_BRIGHTNESS_AMBER);
      break;
    case LIGHT_RED:
    default:
      paintSegment(RED_SEGMENT, 255, 0, 0, PIXEL_BRIGHTNESS_RED);
      break;
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

void forceGreenPhase(const char* source) {
  if (currentState != LIGHT_RED) {
    return;
  }

  Serial.print(source);
  Serial.println(": button request -> forcing GREEN phase");
  currentState = LIGHT_GREEN;
  stateChangedAt = millis();
  showTrafficLight(currentState);
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
  if (!buttons.begin()) {
    stopWithError("Buttons tile not detected. Check Qwiic connections.");
  }
  buttons.setLeds(true, true, true);

  stateChangedAt = millis();
  showTrafficLight(currentState);
  Serial.println("Tiles initialized. Pixels are cycling Red → Green → Yellow.");
  Serial.println("Distance + button events stream below.");
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

void serviceButtons() {
  if (!buttons.update()) {
    return;
  }

  if (buttons.isPressed('A')) {
    forceGreenPhase("Button A");
  }
  if (buttons.isPressed('B')) {
    forceGreenPhase("Button B");
  }
  if (buttons.isPressed('C')) {
    forceGreenPhase("Button C");
  }
}

void loop() {
  unsigned long now = millis();
  updateTrafficLight(now);
  serviceDistanceSensor();
  serviceButtons();
  delay(20);
}
