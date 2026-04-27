#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//config
#define LED_PIN     4
#define NUM_LEDS    12
#define BT_RX       10
#define BT_TX       11
#define TRIG_PIN    13
#define ECHO_PIN    12
#define SERVO_PIN_A 9
#define SERVO_PIN_B 7
#define BUZZER_PIN  3
#define LED_RED_PIN 8
#define LED_GRN_PIN A1
#define DETECT_CM   10

#define MOTOR_A_ENA   6
#define MOTOR_A_IN1   2
#define MOTOR_A_IN2   A0
#define MOTOR_B_ENB   5
#define MOTOR_B_IN3   A2
#define MOTOR_B_IN4   A3
#define MOTOR_SPEED   100

#define SERVO_NEUTRAL    90
#define SERVO_MAX_OFFSET 30
#define SERVO_MIN        (SERVO_NEUTRAL - SERVO_MAX_OFFSET)
#define SERVO_MAX        (SERVO_NEUTRAL + SERVO_MAX_OFFSET)

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
SoftwareSerial bt(BT_RX, BT_TX);
Servo servoA;
Servo servoB;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// big font maps
// each letter is 2 chars wide x 2 rows tall
// custom chars (slots 0-7):
// 0=top-left block, 1=top-right block, 2=bottom-left block, 3=bottom-right block
// 4=full block, 5=bottom bar, 6=left col, 7=right col

byte c_TL[8] = { // top-left of U, W, O
  B11111,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B00000
};
byte c_TR[8] = { // top-right of U, W, O
  B11111,
  B00001,
  B00001,
  B00001,
  B00001,
  B00001,
  B00001,
  B00000
};
byte c_BL_U[8] = { // bottom-left of U
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B11111,
  B00000,
  B00000
};
byte c_BR_U[8] = { // bottom-right of U
  B00001,
  B00001,
  B00001,
  B00001,
  B00001,
  B11111,
  B00000,
  B00000
};
byte c_W_MID[8] = { // middle join of W bottom
  B10001,
  B10001,
  B10001,
  B01110,
  B00000,
  B00000,
  B00000,
  B00000
};
byte c_O_BL[8] = { // bottom-left of O
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B11111,
  B00000,
  B00000
};


// slot assignments:
// 0 = c_TL
// 1 = c_TR
// 2 = c_BL_U  (bottom-left U and O)
// 3 = c_BR_U  (bottom-right U and O)
// 4 = c_W_MID

void initBigFont() {
  lcd.createChar(0, c_TL);
  lcd.createChar(1, c_TR);
  lcd.createChar(2, c_BL_U);
  lcd.createChar(3, c_BR_U);
  lcd.createChar(4, c_W_MID);
}

// print a big 2 wide letter at col position
// row 0 = top row, row 1 = bottom row of LCD
void bigU(int col) {
  lcd.setCursor(col, 0); lcd.write(0); lcd.write(1);
  lcd.setCursor(col, 1); lcd.write(2); lcd.write(3);
}

void bigO(int col) {
  lcd.setCursor(col, 0); lcd.write(0); lcd.write(1);
  lcd.setCursor(col, 1); lcd.write(2); lcd.write(3);
}

void bigW(int col) {
  lcd.setCursor(col, 0); lcd.write(0); lcd.write(1);
  lcd.setCursor(col, 1); lcd.write(4); lcd.write(4);
}

// lowercase versions, just print normal chars centered on row 1 only
void smallu(int col) {
  lcd.setCursor(col, 0); lcd.print("  ");
  lcd.setCursor(col, 1); lcd.print("u ");
}
void smallo(int col) {
  lcd.setCursor(col, 0); lcd.print("  ");
  lcd.setCursor(col, 1); lcd.print("o ");
}
void smallw(int col) {
  lcd.setCursor(col, 0); lcd.print("  ");
  lcd.setCursor(col, 1); lcd.print("w ");
}

void showFace(const char* face) {
  lcd.clear();
  // each face is 3 chars, each big char is 2 cols wide + 1 space = 3 cols
  // total = 9 cols, center on 16 col display: start at col (16-9)/2 = 3
  int startCol = 3;
  for (int i = 0; i < 3; i++) {
    int col = startCol + i * 3;
    char c = face[i];
    if      (c == 'U') bigU(col);
    else if (c == 'u') smallu(col);
    else if (c == 'W') bigW(col);
    else if (c == 'w') smallw(col);
    else if (c == 'O') bigO(col);
    else if (c == 'o') smallo(col);
  }
}

// faces
const char* faces[] = { "UWU", "OWO", "owo", "uwu" };
unsigned long lastFaceChange = 0;
#define FACE_INTERVAL 3000

void randomFace() {
  showFace(faces[random(4)]);
}

// ring light
const uint8_t BRIGHTNESS_PRESETS[4] = { 15, 50, 120, 255 };
uint8_t currentBrightness = 50;

void applyRingLight() {
  for (int i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, strip.Color(currentBrightness, currentBrightness, currentBrightness));
  strip.show();
}

void setBtLed(bool connected) {
  digitalWrite(LED_GRN_PIN, connected ? HIGH : LOW);
  digitalWrite(LED_RED_PIN, connected ? LOW  : HIGH);
}

void writeServos(int posA, int posB) {
  posA = constrain(posA, SERVO_MIN, SERVO_MAX);
  posB = constrain(posB, SERVO_MIN, SERVO_MAX);
  servoA.write(posA);
  servoB.write(posB);
}

void centreServos() { writeServos(SERVO_NEUTRAL, SERVO_NEUTRAL); }

void motorAForward()  { digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW);  analogWrite(MOTOR_A_ENA, MOTOR_SPEED); }
void motorABackward() { digitalWrite(MOTOR_A_IN1, LOW);  digitalWrite(MOTOR_A_IN2, HIGH); analogWrite(MOTOR_A_ENA, MOTOR_SPEED); }
void motorAStop()     { analogWrite(MOTOR_A_ENA, 0); digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, LOW); }

void motorBForward()  { digitalWrite(MOTOR_B_IN3, LOW);  digitalWrite(MOTOR_B_IN4, HIGH); analogWrite(MOTOR_B_ENB, MOTOR_SPEED); }
void motorBBackward() { digitalWrite(MOTOR_B_IN3, HIGH); digitalWrite(MOTOR_B_IN4, LOW);  analogWrite(MOTOR_B_ENB, MOTOR_SPEED); }
void motorBStop()     { analogWrite(MOTOR_B_ENB, 0); digitalWrite(MOTOR_B_IN3, LOW); digitalWrite(MOTOR_B_IN4, LOW); }

void motorsForward()  { motorAForward();  motorBForward();  }
void motorsBackward() { motorABackward(); motorBBackward(); }
void motorsStop()     { motorAStop();     motorBStop();     }

void beep(int freq, int dur) { tone(BUZZER_PIN, freq, dur); }

unsigned long lastPing = 0;
#define PING_INTERVAL 100

long getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return duration * 0.034 / 2;
}

void checkSensor() {
  if (millis() - lastPing < PING_INTERVAL) return;
  lastPing = millis();
  long dist = getDistanceCM();
  if (dist > 0 && dist < DETECT_CM) beep(1500, 100);
}

void handleCmd(char cmd) {
  switch (cmd) {
    case 'W': case 'w': motorsForward();  break;
    case 'S': case 's': motorsBackward(); break;
    case 'Q': case 'q': motorsStop();     break;
    case 'X': case 'x': motorsStop();     break;
    case 'A': case 'a': writeServos(SERVO_MIN, SERVO_MAX); break;
    case 'D': case 'd': writeServos(SERVO_MAX, SERVO_MIN); break;
    case 'E': case 'e': centreServos(); break;
    case 'C': case 'c': centreServos(); setBtLed(true);  break;
    case 'Z': case 'z': setBtLed(false); motorsStop(); centreServos(); break;
    case '1': currentBrightness = BRIGHTNESS_PRESETS[0]; applyRingLight(); break;
    case '2': currentBrightness = BRIGHTNESS_PRESETS[1]; applyRingLight(); break;
    case '3': currentBrightness = BRIGHTNESS_PRESETS[2]; applyRingLight(); break;
    case '4': currentBrightness = BRIGHTNESS_PRESETS[3]; applyRingLight(); break;
  }
}

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  randomSeed(analogRead(A5));

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GRN_PIN, OUTPUT);

  pinMode(MOTOR_A_ENA, OUTPUT); pinMode(MOTOR_A_IN1, OUTPUT); pinMode(MOTOR_A_IN2, OUTPUT); motorAStop();
  pinMode(MOTOR_B_ENB, OUTPUT); pinMode(MOTOR_B_IN3, OUTPUT); pinMode(MOTOR_B_IN4, OUTPUT); motorBStop();

  servoA.attach(SERVO_PIN_A);
  servoB.attach(SERVO_PIN_B);
  centreServos();

  strip.begin();
  applyRingLight();
  setBtLed(false);

  lcd.init();
  lcd.init();
  lcd.backlight();
  initBigFont();
  randomFace();
}

void loop() {
  while (Serial.available()) handleCmd((char)Serial.read());
  while (bt.available()) handleCmd((char)bt.read());
  checkSensor();

  if (millis() - lastFaceChange > FACE_INTERVAL) {
    lastFaceChange = millis();
    randomFace();
  }
}