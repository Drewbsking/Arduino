#include <Arduino_Modulino.h>
#include <Arduino_LED_Matrix.h>
#include <limits.h>

ModulinoDistance distanceSensor;
ModulinoPixels pixels;
ModulinoButtons buttons;
ArduinoLEDMatrix matrix;
uint8_t matrixBuffer[8][12];

const uint8_t WALK_ICON[8][12] = {
  {0,0,0,0,1,1,1,0,0,0,0,0},  // Head
  {0,0,0,1,1,1,1,1,0,0,0,0},  // Shoulders
  {0,0,1,0,0,1,0,0,1,0,0,0},  // Arms spread (walking)
  {0,0,0,0,1,1,1,0,0,0,0,0},  // Torso
  {0,0,0,1,1,1,1,1,0,0,0,0},  // Hips
  {0,0,1,1,0,0,0,1,0,0,0,0},  // Legs in stride
  {0,0,1,0,0,0,0,1,1,0,0,0},  // Lower legs
  {0,1,0,0,0,0,0,0,1,0,0,0}   // Feet
};

const uint8_t STOP_ICON[8][12] = {
  {0,0,1,0,1,0,1,0,1,0,0,0},  // Fingertips
  {0,0,1,0,1,0,1,0,1,0,0,0},  // Fingers
  {0,0,1,0,1,0,1,0,1,0,0,0},  // Fingers
  {0,1,1,1,1,1,1,1,1,1,0,0},  // Top of palm (fingers join)
  {0,1,1,1,1,1,1,1,1,1,0,0},  // Palm
  {0,0,1,1,1,1,1,1,1,0,0,0},  // Palm
  {0,0,1,1,1,1,1,1,1,0,0,0},  // Lower palm
  {0,0,0,1,1,1,1,1,0,0,0,0}   // Wrist
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

// Character font for scrolling text (5 columns x 8 rows)
// Characters needed for "Created by Andrew Bates"
const uint8_t CHAR_FONT[][8][5] = {
  // 'C'
  {{0,1,1,1,1}, {1,0,0,0,0}, {1,0,0,0,0}, {1,0,0,0,0}, {1,0,0,0,0}, {1,0,0,0,0}, {0,1,1,1,1}, {0,0,0,0,0}},
  // 'r'
  {{0,0,0,0,0}, {1,0,1,1,0}, {1,1,0,0,1}, {1,0,0,0,0}, {1,0,0,0,0}, {1,0,0,0,0}, {1,0,0,0,0}, {0,0,0,0,0}},
  // 'e'
  {{0,0,0,0,0}, {0,1,1,1,0}, {1,0,0,0,1}, {1,1,1,1,1}, {1,0,0,0,0}, {1,0,0,0,0}, {0,1,1,1,0}, {0,0,0,0,0}},
  // 'a'
  {{0,0,0,0,0}, {0,1,1,1,0}, {0,0,0,0,1}, {0,1,1,1,1}, {1,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,1}, {0,0,0,0,0}},
  // 't'
  {{0,1,0,0,0}, {0,1,0,0,0}, {1,1,1,1,0}, {0,1,0,0,0}, {0,1,0,0,0}, {0,1,0,0,0}, {0,0,1,1,1}, {0,0,0,0,0}},
  // 'd'
  {{0,0,0,0,1}, {0,0,0,0,1}, {0,1,1,1,1}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,1}, {0,0,0,0,0}},
  // 'b'
  {{1,0,0,0,0}, {1,0,0,0,0}, {1,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {1,1,1,1,0}, {0,0,0,0,0}},
  // 'y'
  {{0,0,0,0,0}, {1,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,1}, {0,0,0,0,1}, {0,0,0,0,1}, {1,1,1,1,0}, {0,0,0,0,0}},
  // 'A'
  {{0,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {1,1,1,1,1}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {0,0,0,0,0}},
  // 'n'
  {{0,0,0,0,0}, {1,0,1,1,0}, {1,1,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {0,0,0,0,0}},
  // 'w'
  {{0,0,0,0,0}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,1,0,1}, {1,1,0,1,1}, {1,0,0,0,1}, {0,0,0,0,0}},
  // 'B'
  {{1,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {1,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {1,1,1,1,0}, {0,0,0,0,0}},
  // 's'
  {{0,0,0,0,0}, {0,1,1,1,1}, {1,0,0,0,0}, {0,1,1,1,0}, {0,0,0,0,1}, {0,0,0,0,1}, {1,1,1,1,0}, {0,0,0,0,0}},
  // ' ' (space)
  {{0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}}
};

const int PIXEL_COUNT = 8;  // 8 LED vertical column
const uint8_t PIXEL_BRIGHTNESS_RED    = 8;
const uint8_t PIXEL_BRIGHTNESS_GREEN  = 8;
const uint8_t PIXEL_BRIGHTNESS_AMBER  = 6;

// Signal A - East/West (top 3 LEDs)
const uint8_t SIGNAL_A_RED    = 7;
const uint8_t SIGNAL_A_YELLOW = 6;
const uint8_t SIGNAL_A_GREEN  = 5;

// Signal B - North (bottom 3 LEDs)
const uint8_t SIGNAL_B_RED    = 2;
const uint8_t SIGNAL_B_YELLOW = 1;
const uint8_t SIGNAL_B_GREEN  = 0;
const int FORCE_GREEN_THRESHOLD_MM = 30;        // force green when an object is this close
const unsigned long MIN_RED_DURATION_MS = 7000; // minimum time red must stay on before allowing green request
const unsigned long MIN_GREEN_NORTH_MS = 7000;  // minimum time North green must stay on
const unsigned long MAX_GREEN_NORTH_MS = 15000; // maximum time North green can stay on
const unsigned long GAP_OUT_TIME_MS = 2000;     // time with no car to end North green (after min)
const unsigned long WALK_SOLID_MS = 3000;
const int WALK_COUNTDOWN_SECONDS = 12;
const unsigned long COUNTDOWN_BLANK_MS = 200;  // Blank display for last 100ms of each second
const unsigned long CAR_COUNT_DISPLAY_MS = 3000;  // Show car count for 5 seconds
const unsigned long FLASH_ON_MS = 600;  // Flash ON time - MUTCD compliant (60 flashes/min, 60% duty)
const unsigned long FLASH_OFF_MS = 400;  // Flash OFF time - MUTCD compliant
const unsigned long LIGHT_SHOW_COLOR_MS = 200;  // Each color displays for 200ms
const unsigned long TEXT_SCROLL_MS = 50;  // Scroll text every 50ms

enum LightState {
  ALL_RED,           // Both signals red
  A_GREEN_B_RED,     // Signal A green, Signal B red
  A_YELLOW_B_RED,    // Signal A yellow, Signal B red
  A_RED_B_GREEN,     // Signal A red, Signal B green
  A_RED_B_YELLOW     // Signal A red, Signal B yellow
};
LightState currentState = ALL_RED;
unsigned long stateChangedAt = 0;
enum PedDisplayMode { PED_STOP, PED_WALK, PED_COUNTDOWN };
PedDisplayMode pedDisplayMode = PED_COUNTDOWN;
int lastCountdownValue = -1;

enum CarState { NO_CAR, CAR_PRESENT };
CarState carState = NO_CAR;
unsigned long carCount = 0;
bool northGreenNext = false;  // Track which direction gets green after ALL_RED
bool pedCountdownActive = false;  // Track if pedestrian countdown is active
unsigned long countdownStartTime = 0;  // When countdown started
unsigned long lastCarDetectedTime = 0;  // Last time a car was detected during North green
bool showingCarCount = false;  // Track if car count is being displayed
unsigned long carCountDisplayStartTime = 0;  // When car count display started

// Flash mode variables (Button B) - MUTCD compliant
bool flashMode = false;  // Track if in flashing caution mode
bool flashState = false;  // Track current flash state (on/off)
unsigned long lastFlashChange = 0;  // When flash last changed state

// Animation mode variables (Button A)
bool animationMode = false;  // Track if in animation mode
enum AnimationPhase { ANIM_LIGHT_SHOW, ANIM_TEXT_SCROLL };
AnimationPhase animationPhase = ANIM_LIGHT_SHOW;
int lightShowColorIndex = 0;  // Current color in the rainbow sequence
unsigned long lastLightShowUpdate = 0;  // When color last changed
int textScrollPosition = 0;  // Horizontal scroll position for text
unsigned long lastTextScrollUpdate = 0;  // When text last scrolled

unsigned long stateDurationMs(LightState state) {
  switch (state) {
    case ALL_RED:        return 2000;  // 2 seconds all red
    case A_GREEN_B_RED:  return ULONG_MAX; // Green stays on until car arrives
    case A_YELLOW_B_RED: return 2000;  // 2 seconds yellow
    case A_RED_B_GREEN:  return ULONG_MAX; // Green stays on until car arrives
    case A_RED_B_YELLOW: return 2000;  // 2 seconds yellow
  }
  return 2000;
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

void blankMatrix() {
  clearMatrixBuffer();
  matrix.renderBitmap(matrixBuffer, 8, 12);
}

void showCarCount(unsigned long count) {
  clearMatrixBuffer();
  // Display car count - handle up to 999 cars
  if (count >= 100) {
    int hundreds = (count / 100) % 10;
    int tens = (count / 10) % 10;
    int ones = count % 10;
    drawDigitToBuffer(hundreds, 0);
    drawDigitToBuffer(tens, 4);
    drawDigitToBuffer(ones, 8);
  } else if (count >= 10) {
    int tens = count / 10;
    int ones = count % 10;
    drawDigitToBuffer(tens, 2);
    drawDigitToBuffer(ones, 7);
  } else {
    drawDigitToBuffer(count, 4);
  }
  matrix.renderBitmap(matrixBuffer, 8, 12);
}

void updatePedestrianDisplay(unsigned long now) {
  // Check if we're displaying car count (button C pressed)
  if (showingCarCount) {
    unsigned long elapsed = now - carCountDisplayStartTime;
    if (elapsed >= CAR_COUNT_DISPLAY_MS) {
      // Timeout - stop showing car count and resume of normal display
      showingCarCount = false;
      lastCountdownValue = -1;  // Force redraw of normal display
    } else {
      // Still showing car count
      return;
    }
  }

  // If not E/W green, show STOP and reset countdown
  if (currentState != A_GREEN_B_RED) {
    if (pedDisplayMode != PED_STOP) {
      renderIcon(STOP_ICON);
      pedDisplayMode = PED_STOP;
      lastCountdownValue = -1;
    }
    pedCountdownActive = false;  // Reset countdown when not E/W green
    return;
  }

  // E/W is green
  if (!pedCountdownActive) {
    // No countdown active - show WALK solid
    if (pedDisplayMode != PED_WALK) {
      renderIcon(WALK_ICON);
      pedDisplayMode = PED_WALK;
      lastCountdownValue = -1;
    }
  } else {
    // Countdown active - calculate remaining time using current time
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - countdownStartTime;
    int remaining = WALK_COUNTDOWN_SECONDS - (int)(elapsed / 1000);
    unsigned long msWithinSecond = elapsed % 1000;

    Serial.print(F("[PED DEBUG] elapsed="));
    Serial.print(elapsed);
    Serial.print(F("ms, remaining="));
    Serial.println(remaining);

    if (remaining > 0) {
      // Check if we should blank the display (last 200ms of each second)
      bool shouldBlank = (msWithinSecond >= (1000 - COUNTDOWN_BLANK_MS));

      if (shouldBlank) {
        // Blank the display for visual pulse effect
        if (pedDisplayMode != PED_COUNTDOWN || lastCountdownValue != -2) {
          blankMatrix();
          lastCountdownValue = -2;  // Use -2 to indicate blank state
        }
      } else {
        // Show countdown number
        if (pedDisplayMode != PED_COUNTDOWN || remaining != lastCountdownValue) {
          showCountdownNumber(remaining);
          pedDisplayMode = PED_COUNTDOWN;
          lastCountdownValue = remaining;
        }
      }
    } else {
      // Countdown finished - show STOP and trigger E/W yellow
      if (pedDisplayMode != PED_STOP) {
        renderIcon(STOP_ICON);
        pedDisplayMode = PED_STOP;
        lastCountdownValue = -1;
        pedCountdownActive = false;
        setPhase(A_YELLOW_B_RED, "Pedestrian countdown finished");
      }
    }
  }
}

void updateFlashMode(unsigned long now) {
  // MUTCD-compliant flash mode: 60 flashes/min with 60% duty cycle
  unsigned long elapsed = now - lastFlashChange;
  unsigned long targetTime = flashState ? FLASH_ON_MS : FLASH_OFF_MS;

  if (elapsed >= targetTime) {
    flashState = !flashState;
    lastFlashChange = now;

    clearPixels();
    if (flashState) {
      // Lights ON: E/W amber, North red
      pixels.set(SIGNAL_A_YELLOW, 255, 110, 0, PIXEL_BRIGHTNESS_AMBER);
      pixels.set(SIGNAL_B_RED, 255, 0, 0, PIXEL_BRIGHTNESS_RED);
    }
    // Lights OFF: all dark (already cleared)
    pixels.show();
  }
}

void updateLightShow(unsigned long now) {
  // Cycle through colors every LIGHT_SHOW_COLOR_MS
  if (now - lastLightShowUpdate >= LIGHT_SHOW_COLOR_MS) {
    lastLightShowUpdate = now;

    // Rainbow colors: Red, Orange, Yellow, Green, Cyan, Blue, Purple, White
    uint8_t r = 0, g = 0, b = 0;
    switch(lightShowColorIndex) {
      case 0: r = 255; g = 0; b = 0; break;      // Red
      case 1: r = 255; g = 110; b = 0; break;    // Orange
      case 2: r = 255; g = 255; b = 0; break;    // Yellow
      case 3: r = 0; g = 255; b = 0; break;      // Green
      case 4: r = 0; g = 255; b = 255; break;    // Cyan
      case 5: r = 0; g = 0; b = 255; break;      // Blue
      case 6: r = 255; g = 0; b = 255; break;    // Purple
      case 7: r = 255; g = 255; b = 255; break;  // White
    }

    // Set all 8 LEDs to the same color
    clearPixels();
    for (int i = 0; i < PIXEL_COUNT; i++) {
      pixels.set(i, r, g, b, 8);
    }
    pixels.show();

    // Advance to next color (0-7 cycle)
    lightShowColorIndex++;
    if (lightShowColorIndex >= 8) {
      // Completed a full rainbow cycle - switch to text scroll phase
      lightShowColorIndex = 0;
      animationPhase = ANIM_TEXT_SCROLL;
      textScrollPosition = 12;  // Start text off-screen to the right
      lastTextScrollUpdate = now;
      clearPixels();
      pixels.show();
      Serial.println(F("[ANIM] Light show complete - starting text scroll"));
    }
  }
}

// Character index lookup for "Created by Andrew Bates"
int getCharIndex(char c) {
  switch(c) {
    case 'C': return 0;
    case 'r': return 1;
    case 'e': return 2;
    case 'a': return 3;
    case 't': return 4;
    case 'd': return 5;
    case 'b': return 6;
    case 'y': return 7;
    case 'A': return 8;
    case 'n': return 9;
    case 'w': return 10;
    case 'B': return 11;
    case 's': return 12;
    case ' ': return 13;
    default: return 13; // Return space for unknown characters
  }
}

void updateTextScroll(unsigned long now) {
  // Scroll text every TEXT_SCROLL_MS
  if (now - lastTextScrollUpdate >= TEXT_SCROLL_MS) {
    lastTextScrollUpdate = now;

    const char* message = "Created by Andrew Bates";
    int messageLen = strlen(message);

    // Clear the display buffer
    clearMatrixBuffer();

    // Calculate total width of message (each char is 5 pixels + 1 pixel spacing)
    int totalWidth = messageLen * 6;  // 5 pixels per char + 1 spacing

    // Render visible portion of the message
    int xPos = textScrollPosition;
    for (int i = 0; i < messageLen; i++) {
      int charIndex = getCharIndex(message[i]);

      // Draw this character's columns
      for (int col = 0; col < 5; col++) {
        int screenCol = xPos + col;
        if (screenCol >= 0 && screenCol < 12) {
          // This column is visible on screen
          for (int row = 0; row < 8; row++) {
            matrixBuffer[row][screenCol] = CHAR_FONT[charIndex][row][col];
          }
        }
      }
      xPos += 6;  // Move to next character position (5 pixels + 1 spacing)
    }

    // Render to matrix
    matrix.renderBitmap(matrixBuffer, 8, 12);

    // Move scroll position left
    textScrollPosition--;

    // Check if scroll is complete (entire message has scrolled off left side)
    if (textScrollPosition < -totalWidth) {
      // Text scroll complete - loop back to light show
      animationPhase = ANIM_LIGHT_SHOW;
      lightShowColorIndex = 0;
      lastLightShowUpdate = now;
      Serial.println(F("[ANIM] Text scroll complete - restarting light show"));
    }
  }
}

void showTrafficLight(LightState state) {
  clearPixels();

  switch (state) {
    case ALL_RED:
      pixels.set(SIGNAL_A_RED, 255, 0, 0, PIXEL_BRIGHTNESS_RED);
      pixels.set(SIGNAL_B_RED, 255, 0, 0, PIXEL_BRIGHTNESS_RED);
      break;

    case A_GREEN_B_RED:
      pixels.set(SIGNAL_A_GREEN, 0, 255, 0, PIXEL_BRIGHTNESS_GREEN);
      pixels.set(SIGNAL_B_RED, 255, 0, 0, PIXEL_BRIGHTNESS_RED);
      break;

    case A_YELLOW_B_RED:
      pixels.set(SIGNAL_A_YELLOW, 255, 110, 0, PIXEL_BRIGHTNESS_AMBER);
      pixels.set(SIGNAL_B_RED, 255, 0, 0, PIXEL_BRIGHTNESS_RED);
      break;

    case A_RED_B_GREEN:
      pixels.set(SIGNAL_A_RED, 255, 0, 0, PIXEL_BRIGHTNESS_RED);
      pixels.set(SIGNAL_B_GREEN, 0, 255, 0, PIXEL_BRIGHTNESS_GREEN);
      break;

    case A_RED_B_YELLOW:
      pixels.set(SIGNAL_A_RED, 255, 0, 0, PIXEL_BRIGHTNESS_RED);
      pixels.set(SIGNAL_B_YELLOW, 255, 110, 0, PIXEL_BRIGHTNESS_AMBER);
      break;
  }

  pixels.show();
}

const char* stateName(LightState state) {
  switch (state) {
    case ALL_RED:        return "ALL RED";
    case A_GREEN_B_RED:  return "E/W:GREEN N:RED";
    case A_YELLOW_B_RED: return "E/W:YELLOW N:RED";
    case A_RED_B_GREEN:  return "E/W:RED N:GREEN";
    case A_RED_B_YELLOW: return "E/W:RED N:YELLOW";
    default:             return "UNKNOWN";
  }
}

void setPhase(LightState next, const char* reason) {
  currentState = next;
  stateChangedAt = millis();
  showTrafficLight(currentState);
  updatePedestrianDisplay(stateChangedAt);

  // Initialize car detection time when North green starts
  if (next == A_RED_B_GREEN) {
    lastCarDetectedTime = stateChangedAt;
  }

  Serial.print(F("[STATE] "));
  if (reason != nullptr) {
    Serial.print(reason);
    Serial.print(F(" -> "));
  }
  Serial.println(stateName(currentState));
}

void advanceTrafficLight() {
  LightState next;
  switch (currentState) {
    case ALL_RED:
      // Give green to whichever direction is waiting
      if (northGreenNext) {
        next = A_RED_B_GREEN;  // North gets green
        northGreenNext = false;
      } else {
        next = A_GREEN_B_RED;  // E/W gets green (default)
      }
      break;
    case A_GREEN_B_RED:
      next = A_YELLOW_B_RED;
      break;
    case A_YELLOW_B_RED:
      next = ALL_RED;
      northGreenNext = true;  // North gets green next
      break;
    case A_RED_B_GREEN:
      next = A_RED_B_YELLOW;
      break;
    case A_RED_B_YELLOW:
      next = ALL_RED;
      northGreenNext = false;  // E/W gets green next (back to default)
      break;
    default:
      next = ALL_RED;
      break;
  }
  setPhase(next, "Timer");
}

void updateTrafficLight(unsigned long now) {
  // E/W Green stays on indefinitely until a car arrives
  if (currentState == A_GREEN_B_RED) {
    return;
  }

  // North green has maximum time limit
  if (currentState == A_RED_B_GREEN) {
    unsigned long greenDuration = now - stateChangedAt;
    if (greenDuration >= MAX_GREEN_NORTH_MS) {
      Serial.print(F("[TIMER] North green maximum reached ("));
      Serial.print(MAX_GREEN_NORTH_MS / 1000);
      Serial.println(F("s) - forcing yellow"));
      setPhase(A_RED_B_YELLOW, "North max green time");
    }
    return;
  }

  if (now - stateChangedAt >= stateDurationMs(currentState)) {
    advanceTrafficLight();
  }
}

void requestGreenPhase(const char* source) {
  // Only grant requests when Signal A is red (ALL_RED or A_RED_B_GREEN states)
  if (currentState != ALL_RED && currentState != A_RED_B_GREEN && currentState != A_RED_B_YELLOW) {
    return;
  }

  unsigned long elapsed = millis() - stateChangedAt;
  if (elapsed >= MIN_RED_DURATION_MS) {
    setPhase(A_GREEN_B_RED, source);
  } else {
    Serial.print(F("[REQUEST] Green request from "));
    Serial.print(source);
    Serial.print(F(" denied - red minimum not met ("));
    Serial.print(elapsed / 1000);
    Serial.print(F("s / "));
    Serial.print(MIN_RED_DURATION_MS / 1000);
    Serial.println(F("s)"));
  }
}

void forceGreenPhase(const char* source) {
  // Force Signal A to green if it's currently red
  if (currentState == ALL_RED || currentState == A_RED_B_GREEN || currentState == A_RED_B_YELLOW) {
    setPhase(A_GREEN_B_RED, source);
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

  setPhase(A_GREEN_B_RED, "Startup");
  Serial.println("Tiles initialized. Two-phase traffic signal active.");
  Serial.println("E/W signal is main road (default green). N signal actuated by sensor.");
  Serial.println("Distance + button events stream below.");
}

void serviceDistanceSensor() {
  if (!distanceSensor.available()) {
    return;
  }

  int measure = distanceSensor.get();
  bool carPresent = (measure > 0 && measure <= FORCE_GREEN_THRESHOLD_MM);

  // Detect state transitions
  if (carPresent && carState == NO_CAR) {
    // North car arrival detected
    carState = CAR_PRESENT;
    Serial.print(F("[CAR] North arrival detected (distance: "));
    Serial.print(measure);
    Serial.println(F(" mm)"));

    // Track car detection time during North green (for extension logic)
    if (currentState == A_RED_B_GREEN) {
      lastCarDetectedTime = millis();
      unsigned long greenDuration = lastCarDetectedTime - stateChangedAt;
      Serial.print(F("[CAR] North green extended - duration: "));
      Serial.print(greenDuration / 1000);
      Serial.println(F("s"));
    }

    // If E/W is green, start pedestrian countdown (don't immediately trigger yellow)
    if (currentState == A_GREEN_B_RED && !pedCountdownActive) {
      pedCountdownActive = true;
      countdownStartTime = millis();
      Serial.println(F("[PED] Countdown started for E/W pedestrians"));
    }
  }
  else if (!carPresent && carState == CAR_PRESENT) {
    // North car departure detected
    carState = NO_CAR;
    carCount++;
    Serial.print(F("[CAR] North departure detected - Total cars: "));
    Serial.println(carCount);

    // If North is green, check if we can end the green phase
    if (currentState == A_RED_B_GREEN) {
      unsigned long now = millis();
      unsigned long greenDuration = now - stateChangedAt;
      unsigned long timeSinceLastCar = now - lastCarDetectedTime;

      // Must meet minimum green time
      if (greenDuration >= MIN_GREEN_NORTH_MS) {
        // After minimum, end green if gap time exceeded or approaching max
        if (timeSinceLastCar >= GAP_OUT_TIME_MS || greenDuration >= MAX_GREEN_NORTH_MS) {
          Serial.print(F("[CAR] North green ending - duration: "));
          Serial.print(greenDuration / 1000);
          Serial.println(F("s"));
          setPhase(A_RED_B_YELLOW, "North car departing");
        } else {
          Serial.print(F("[CAR] North green extended - recent car detected "));
          Serial.print(timeSinceLastCar / 1000);
          Serial.println(F("s ago"));
        }
      } else {
        Serial.print(F("[CAR] North car departed but minimum green not met ("));
        Serial.print(greenDuration / 1000);
        Serial.print(F("s / "));
        Serial.print(MIN_GREEN_NORTH_MS / 1000);
        Serial.println(F("s)"));
      }
    }
  }
}

void serviceButtons() {
  if (!buttons.update()) {
    return;
  }

  if (buttons.isPressed('A')) {
    // Button A toggles animation mode (light show + text scroll)
    animationMode = !animationMode;
    if (animationMode) {
      Serial.println(F("[ANIM] Entering animation mode - light show starting"));
      animationPhase = ANIM_LIGHT_SHOW;
      lightShowColorIndex = 0;
      lastLightShowUpdate = millis();
    } else {
      Serial.println(F("[ANIM] Exiting animation mode - returning to normal operation"));
      // Exit animation mode: go to ALL_RED then E/W green
      setPhase(ALL_RED, "Animation mode exit");
      northGreenNext = false;  // Ensure E/W gets green next
    }
  }
  if (buttons.isPressed('B')) {
    // Button B toggles flash mode - MUTCD compliant
    flashMode = !flashMode;
    if (flashMode) {
      Serial.println(F("[FLASH] Entering flash mode - E/W amber, N red (MUTCD 60 flashes/min)"));
      lastFlashChange = millis();
      flashState = false;  // Start with lights off
    } else {
      Serial.println(F("[FLASH] Exiting flash mode - returning to normal operation"));
      // Exit flash mode: go to ALL_RED then E/W green
      setPhase(ALL_RED, "Flash mode exit");
      northGreenNext = false;  // Ensure E/W gets green next
    }
  }
  if (buttons.isPressed('C')) {
    // Button C shows car count on LED matrix
    showingCarCount = true;
    carCountDisplayStartTime = millis();
    showCarCount(carCount);
    Serial.print(F("[INFO] Displaying car count: "));
    Serial.println(carCount);
  }
}

void loop() {
  unsigned long now = millis();

  serviceButtons();  // Check buttons first

  if (animationMode) {
    // In animation mode - run light show and text scroll
    if (animationPhase == ANIM_LIGHT_SHOW) {
      updateLightShow(now);
    } else {
      updateTextScroll(now);
    }
  } else if (flashMode) {
    // In flash mode - MUTCD compliant flashing
    updateFlashMode(now);
    // Blank the LED matrix during flash mode
    if (pedDisplayMode != PED_STOP || !showingCarCount) {
      blankMatrix();
      pedDisplayMode = PED_STOP;
    }
  } else {
    // Normal traffic signal operation
    updateTrafficLight(now);
    serviceDistanceSensor();
    updatePedestrianDisplay(now);
  }

  delay(20);
}
