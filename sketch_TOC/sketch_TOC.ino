#include <Arduino_Modulino.h>
#include <Arduino_LED_Matrix.h>

ModulinoDistance distanceSensor;
ModulinoPixels pixels;
ModulinoButtons buttons;
ArduinoLEDMatrix matrix;
uint8_t matrixBuffer[8][12];

const uint8_t WALK_ICON[8][12] = {
  {0,0,0,1,1,1,1,0,0,0,0,0},
  {0,0,0,1,0,0,1,0,0,0,0,0},
  {0,0,0,1,1,1,1,0,0,0,0,0},
  {0,0,1,1,1,1,1,1,0,0,0,0},
  {0,0,0,0,1,1,0,0,0,0,0,0},
  {0,0,0,0,1,1,0,0,1,0,0,0},
  {0,0,0,1,1,1,0,1,0,0,0,0},
  {0,0,1,0,0,0,1,0,0,0,0,0}
};

const uint8_t STOP_ICON[8][12] = {
  {0,0,1,1,1,1,1,1,1,0,0,0},
  {0,1,1,1,1,1,1,1,1,1,0,0},
  {0,1,1,1,1,1,1,1,1,1,0,0},
  {0,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,1,1,1,1,1,1,1,0,0,0},
  {0,0,1,1,1,1,1,1,1,0,0,0},
  {0,0,1,1,1,1,1,1,1,0,0,0},
  {0,0,1,1,1,1,1,1,1,0,0,0}
};

const uint8_t DIGIT_FONT[10][7][5] = {
  { // 0
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,1,1,1,1}
  },
  { // 1
    {0,0,1,0,0},
    {0,1,1,0,0},
    {0,0,1,0,0},
    {0,0,1,0,0},
    {0,0,1,0,0},
    {0,0,1,0,0},
    {0,1,1,1,0}
  },
  { // 2
    {1,1,1,1,1},
    {0,0,0,0,1},
    {0,0,0,0,1},
    {1,1,1,1,1},
    {1,0,0,0,0},
    {1,0,0,0,0},
    {1,1,1,1,1}
  },
  { // 3
    {1,1,1,1,1},
    {0,0,0,0,1},
    {0,0,0,0,1},
    {0,1,1,1,1},
    {0,0,0,0,1},
    {0,0,0,0,1},
    {1,1,1,1,1}
  },
  { // 4
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,1,1,1,1},
    {0,0,0,0,1},
    {0,0,0,0,1},
    {0,0,0,0,1}
  },
  { // 5
    {1,1,1,1,1},
    {1,0,0,0,0},
    {1,0,0,0,0},
    {1,1,1,1,1},
    {0,0,0,0,1},
    {0,0,0,0,1},
    {1,1,1,1,1}
  },
  { // 6
    {1,1,1,1,1},
    {1,0,0,0,0},
    {1,0,0,0,0},
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,1,1,1,1}
  },
  { // 7
    {1,1,1,1,1},
    {0,0,0,0,1},
    {0,0,0,1,0},
    {0,0,1,0,0},
    {0,1,0,0,0},
    {0,1,0,0,0},
    {0,1,0,0,0}
  },
  { // 8
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,1,1,1,1}
  },
  { // 9
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,0,0,0,1},
    {1,1,1,1,1},
    {0,0,0,0,1},
    {0,0,0,0,1},
    {1,1,1,1,1}
  }
};

const int PIXEL_COUNT = 9;  // Modulino Pixels is a 3x3 tile
const uint8_t PIXEL_BRIGHTNESS_RED    = 8;
const uint8_t PIXEL_BRIGHTNESS_GREEN  = 8;
const uint8_t PIXEL_BRIGHTNESS_AMBER  = 6;

const uint8_t RED_SEGMENT[]   = {0, 1, 2};      // bottom row
const uint8_t AMBER_SEGMENT[] = {3, 4, 5};      // middle row
const uint8_t GREEN_SEGMENT[] = {6, 7, 8};      // top row
const int FORCE_GREEN_THRESHOLD_MM = 20;        // force green when an object is this close
const unsigned long WALK_SOLID_MS = 3000;
const int WALK_COUNTDOWN_SECONDS = 12;

enum LightState { LIGHT_RED, LIGHT_GREEN, LIGHT_YELLOW };
LightState currentState = LIGHT_RED;
unsigned long stateChangedAt = 0;
enum PedDisplayMode { PED_STOP, PED_WALK, PED_COUNTDOWN };
PedDisplayMode pedDisplayMode = PED_COUNTDOWN;
int lastCountdownValue = -1;

unsigned long stateDurationMs(LightState state) {
  switch (state) {
    case LIGHT_RED:    return 15000; // 15 seconds of red
    case LIGHT_GREEN:  return 15000; // 15 seconds of green
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

void copyIconToBuffer(const uint8_t icon[8][12]) {
  for (int r = 0; r < 8; ++r) {
    for (int c = 0; c < 12; ++c) {
      matrixBuffer[r][c] = icon[r][c];
    }
  }
}

void renderIcon(const uint8_t icon[8][12]) {
  copyIconToBuffer(icon);
  matrix.renderBitmap(matrixBuffer, 8, 12);
}

void clearMatrixBuffer() {
  for (int r = 0; r < 8; ++r) {
    for (int c = 0; c < 12; ++c) {
      matrixBuffer[r][c] = 0;
    }
  }
}

void drawDigitToBuffer(int digit, int colOffset) {
  if (digit < 0 || digit > 9) return;
  for (int r = 0; r < 7; ++r) {
    for (int c = 0; c < 5; ++c) {
      int rr = r + 1; // vertical centering
      int cc = colOffset + c;
      if (cc >= 0 && cc < 12) {
        matrixBuffer[rr][cc] = DIGIT_FONT[digit][r][c];
      }
    }
  }
}

void showCountdownNumber(int value) {
  clearMatrixBuffer();
  if (value >= 10) {
    int tens = value / 10;
    int ones = value % 10;
    drawDigitToBuffer(tens, 0);
    drawDigitToBuffer(ones, 6);
  } else {
    drawDigitToBuffer(value, 3);
  }
  matrix.renderBitmap(matrixBuffer, 8, 12);
}

void updatePedestrianDisplay(unsigned long now) {
  if (currentState != LIGHT_GREEN) {
    if (pedDisplayMode != PED_STOP) {
      renderIcon(STOP_ICON);
      pedDisplayMode = PED_STOP;
      lastCountdownValue = -1;
    }
    return;
  }

  unsigned long elapsed = now - stateChangedAt;
  if (elapsed < WALK_SOLID_MS) {
    if (pedDisplayMode != PED_WALK) {
      renderIcon(WALK_ICON);
      pedDisplayMode = PED_WALK;
      lastCountdownValue = WALK_COUNTDOWN_SECONDS;
    }
    return;
  }

  unsigned long countdownElapsed = elapsed - WALK_SOLID_MS;
  int remaining = WALK_COUNTDOWN_SECONDS - (int)(countdownElapsed / 1000);
  if (remaining < 0) remaining = 0;

  if (pedDisplayMode != PED_COUNTDOWN || remaining != lastCountdownValue) {
    showCountdownNumber(remaining);
    pedDisplayMode = PED_COUNTDOWN;
    lastCountdownValue = remaining;
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

const char* stateName(LightState state) {
  switch (state) {
    case LIGHT_GREEN:  return "GREEN";
    case LIGHT_YELLOW: return "YELLOW";
    case LIGHT_RED:
    default:           return "RED";
  }
}

void setPhase(LightState next, const char* reason) {
  currentState = next;
  stateChangedAt = millis();
  showTrafficLight(currentState);
  updatePedestrianDisplay(stateChangedAt);

  Serial.print(F("[STATE] "));
  if (reason != nullptr) {
    Serial.print(reason);
    Serial.print(F(" -> "));
  }
  Serial.println(stateName(currentState));
}

void advanceTrafficLight() {
  LightState next;
  if (currentState == LIGHT_RED) {
    next = LIGHT_GREEN;
  } else if (currentState == LIGHT_GREEN) {
    next = LIGHT_YELLOW;
  } else {
    next = LIGHT_RED;
  }
  setPhase(next, "Timer");
}

void updateTrafficLight(unsigned long now) {
  if (now - stateChangedAt >= stateDurationMs(currentState)) {
    advanceTrafficLight();
  }
}

void requestGreenPhase(const char* source) {
  if (currentState == LIGHT_RED) {
    setPhase(LIGHT_GREEN, source);
  }
}

void stopWithError(const char* message) {
  Serial.println(message);
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(9600);
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {
    delay(10);
  }

  matrix.begin();

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

  setPhase(LIGHT_RED, "Startup");
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
  bool tooClose = (measure > 0 && measure <= FORCE_GREEN_THRESHOLD_MM);
  if (tooClose) {
    Serial.print(" mm  <-- within ");
    Serial.print(FORCE_GREEN_THRESHOLD_MM);
    Serial.println(" mm (requesting GREEN)");
    requestGreenPhase("Distance sensor");
  } else {
    Serial.println(" mm");
  }
}

void serviceButtons() {
  if (!buttons.update()) {
    return;
  }

  if (buttons.isPressed('A')) {
    requestGreenPhase("Button A");
  }
  if (buttons.isPressed('B')) {
    requestGreenPhase("Button B");
  }
  if (buttons.isPressed('C')) {
    requestGreenPhase("Button C");
  }
}

void loop() {
  unsigned long now = millis();
  updateTrafficLight(now);
  serviceDistanceSensor();
  serviceButtons();
  updatePedestrianDisplay(now);
  delay(20);
}
