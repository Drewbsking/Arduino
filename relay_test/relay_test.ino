/*
 * Relay Test Sketch - Proof of Concept
 *
 * Simple program to test 4-channel relay module with traffic signal
 * Cycles through: GREEN -> YELLOW -> RED -> repeat
 * LED Matrix displays current phase: G, Y, or R
 *
 * ⚠️ WARNING: RELAYS CONTROL 120V AC - HIGH VOLTAGE - LETHAL!
 * - Only wire high voltage with power OFF at breaker
 * - Use proper electrical enclosure
 * - Follow all local electrical codes
 * - Consider hiring licensed electrician
 * - Test wiring with multimeter before powering on
 *
 * Wiring:
 * Arduino 5V   -> Relay VCC Brown Wire
 * Arduino GND  -> Relay GND White with Brown Trace
 * Arduino Pin 9  -> Relay IN1 (Red) Blue Wire
 * Arduino Pin 10 -> Relay IN2 (Yellow) Orange Wire
 * Arduino Pin 11 -> Relay IN3 (Green) Green Wire
 */

#include <Arduino_LED_Matrix.h>

ArduinoLEDMatrix matrix;
uint8_t matrixBuffer[8][12];

// Large letter definitions for LED matrix (8 rows x 12 columns)
const uint8_t LETTER_R[8][12] = {
  {0,1,1,1,1,1,1,0,0,0,0,0},  // RRRRRR
  {0,1,1,0,0,0,1,1,0,0,0,0},  // RR   RR
  {0,1,1,0,0,0,1,1,0,0,0,0},  // RR   RR
  {0,1,1,1,1,1,1,0,0,0,0,0},  // RRRRRR
  {0,1,1,1,1,0,0,0,0,0,0,0},  // RRRR
  {0,1,1,0,1,1,0,0,0,0,0,0},  // RR RR
  {0,1,1,0,0,1,1,0,0,0,0,0},  // RR  RR
  {0,1,1,0,0,0,1,1,0,0,0,0}   // RR   RR
};

const uint8_t LETTER_Y[8][12] = {
  {0,1,1,0,0,0,0,1,1,0,0,0},  // YY    YY
  {0,1,1,0,0,0,0,1,1,0,0,0},  // YY    YY
  {0,0,1,1,0,0,1,1,0,0,0,0},  //  YY  YY
  {0,0,0,1,1,1,1,0,0,0,0,0},  //   YYYY
  {0,0,0,0,1,1,0,0,0,0,0,0},  //    YY
  {0,0,0,0,1,1,0,0,0,0,0,0},  //    YY
  {0,0,0,0,1,1,0,0,0,0,0,0},  //    YY
  {0,0,0,0,1,1,0,0,0,0,0,0}   //    YY
};

const uint8_t LETTER_G[8][12] = {
  {0,0,1,1,1,1,1,1,0,0,0,0},  //  GGGGGG
  {0,1,1,0,0,0,0,1,1,0,0,0},  // GG    GG
  {0,1,1,0,0,0,0,0,0,0,0,0},  // GG
  {0,1,1,0,0,1,1,1,1,0,0,0},  // GG  GGGG
  {0,1,1,0,0,0,0,1,1,0,0,0},  // GG    GG
  {0,1,1,0,0,0,0,1,1,0,0,0},  // GG    GG
  {0,1,1,0,0,0,0,1,1,0,0,0},  // GG    GG
  {0,0,1,1,1,1,1,1,0,0,0,0}   //  GGGGGG
};

// Relay control pins
const uint8_t RELAY_RED    = 9;   // Controls red light
const uint8_t RELAY_YELLOW = 10;  // Controls yellow light
const uint8_t RELAY_GREEN  = 11;  // Controls green light

// Timing for each light (in milliseconds)
const unsigned long GREEN_TIME  = 5000;  // 5 seconds green
const unsigned long YELLOW_TIME = 2000;  // 2 seconds yellow
const unsigned long RED_TIME    = 5000;  // 5 seconds red

void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  while (!Serial && millis() < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.println("=================================");
  Serial.println("   Relay Test - Traffic Signal");
  Serial.println("=================================");
  Serial.println();
  Serial.println("⚠️  HIGH VOLTAGE WARNING");
  Serial.println("Ensure all wiring is complete");
  Serial.println("and safe before connecting power!");
  Serial.println();

  // Initialize LED matrix
  matrix.begin();

  // Initialize relay pins as outputs
  pinMode(RELAY_RED, OUTPUT);
  pinMode(RELAY_YELLOW, OUTPUT);
  pinMode(RELAY_GREEN, OUTPUT);

  // Start with all relays OFF
  digitalWrite(RELAY_RED, LOW);
  digitalWrite(RELAY_YELLOW, LOW);
  digitalWrite(RELAY_GREEN, LOW);

  Serial.println("Relay pins initialized");
  Serial.println("LED matrix initialized");
  Serial.println("Starting light cycle in 3 seconds...");
  Serial.println();

  delay(3000);
}

void displayLetter(const uint8_t letter[8][12]) {
  // Copy letter pattern to matrix buffer and display it
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 12; c++) {
      matrixBuffer[r][c] = letter[r][c];
    }
  }
  matrix.renderBitmap(matrixBuffer, 8, 12);
}

void turnOffAllLights() {
  digitalWrite(RELAY_RED, LOW);
  digitalWrite(RELAY_YELLOW, LOW);
  digitalWrite(RELAY_GREEN, LOW);
}

void showGreen() {
  turnOffAllLights();
  digitalWrite(RELAY_GREEN, HIGH);
  displayLetter(LETTER_G);
  Serial.println("[GREEN] Light ON - Matrix shows 'G'");
}

void showYellow() {
  turnOffAllLights();
  digitalWrite(RELAY_YELLOW, HIGH);
  displayLetter(LETTER_Y);
  Serial.println("[YELLOW] Light ON - Matrix shows 'Y'");
}

void showRed() {
  turnOffAllLights();
  digitalWrite(RELAY_RED, HIGH);
  displayLetter(LETTER_R);
  Serial.println("[RED] Light ON - Matrix shows 'R'");
}

void loop() {
  // GREEN light
  showGreen();
  delay(GREEN_TIME);

  // YELLOW light
  showYellow();
  delay(YELLOW_TIME);

  // RED light
  showRed();
  delay(RED_TIME);

  Serial.println("---");  // Separator for each cycle
}
