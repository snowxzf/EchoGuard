/*
 * EchoGuard — Ultrasonic Haptic Navigation Headband  (Arduino Uno FALLBACK)
 * ------------------------------------------------------------
 * This is the 5 V Arduino Uno version, kept as a fallback. The ESP32 version in
 * the parent folder (EchoGuard.ino) is the primary build.
 *
 * On the Uno everything is 5 V, so a plain HC-SR04 wires in directly (no voltage
 * divider). Board: Arduino Uno. Serial @ 9600. No external libraries required.
 * ------------------------------------------------------------
 */

// ======================= CONFIG (edit these) =======================
#define NUM_SENSORS   1      // 1 for first tests, 3 for the full headband
#define USE_BUZZER    1      // 1 = audible feedback (best for the very first test)
#define USE_MOTORS    0      // 1 = drive vibration motors via PWM
#define SERIAL_DEBUG  1      // 1 = print distances + loop time to the Serial Monitor

const int MAX_DISTANCE_CM = 100;  // start reacting at/below this distance
const int MIN_DISTANCE_CM = 8;    // at/below this = maximum intensity (clamp)

const int MOTOR_MIN_PWM = 100;    // lowest duty that still makes the motor spin
const int MOTOR_MAX_PWM = 180;    // cap — protects ~3 V motors when driven from 5 V

const int BUZZ_MIN_HZ = 200;
const int BUZZ_MAX_HZ = 2500;

const int FILTER_N = 4;
const unsigned long ECHO_TIMEOUT_US = 25000UL;
const int SETTLE_MS = 10;

// Pin map (Arduino Uno). Motors avoid 3 & 11 (tone() uses Timer2 -> PWM on 3/11).
const int  TRIG_PINS[3]  = {2, 4, 7};
const int  ECHO_PINS[3]  = {3, 6, 8};
const int  MOTOR_PINS[3] = {9, 10, 5};   // PWM (~) pins
const int  BUZZER_PIN    = 12;
const char* ZONE_NAME[3] = {"FRONT", "LEFT", "RIGHT"};

long fbuf[3][FILTER_N];
int  fidx[3];

void setup() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);
    for (int k = 0; k < FILTER_N; k++) fbuf[i][k] = MAX_DISTANCE_CM;
    fidx[i] = 0;
#if USE_MOTORS
    pinMode(MOTOR_PINS[i], OUTPUT);
    analogWrite(MOTOR_PINS[i], 0);
#endif
  }
#if USE_BUZZER
  pinMode(BUZZER_PIN, OUTPUT);
#endif
#if SERIAL_DEBUG
  Serial.begin(9600);
  Serial.println(F("EchoGuard (Uno) starting..."));
#endif
}

long readDistanceCM(int i) {
  int trig = TRIG_PINS[i], echo = ECHO_PINS[i];
  digitalWrite(trig, LOW);  delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long dur = pulseIn(echo, HIGH, ECHO_TIMEOUT_US);
  if (dur == 0) return -1;
  return (long)(dur * 0.0343 / 2.0);
}

long filtered(int i, long raw) {
  long v = (raw < 0) ? MAX_DISTANCE_CM : raw;
  fbuf[i][fidx[i]] = v;
  fidx[i] = (fidx[i] + 1) % FILTER_N;
  long sum = 0;
  for (int k = 0; k < FILTER_N; k++) sum += fbuf[i][k];
  return sum / FILTER_N;
}

int distanceToStrength(long cm) {
  if (cm >= MAX_DISTANCE_CM) return 0;
  if (cm < MIN_DISTANCE_CM) cm = MIN_DISTANCE_CM;
  return map(cm, MIN_DISTANCE_CM, MAX_DISTANCE_CM, MOTOR_MAX_PWM, MOTOR_MIN_PWM);
}

void loop() {
  unsigned long t0 = millis();
  long nearest = MAX_DISTANCE_CM;

  for (int i = 0; i < NUM_SENSORS; i++) {
    long d = filtered(i, readDistanceCM(i));
    if (d < nearest) nearest = d;
#if USE_MOTORS
    analogWrite(MOTOR_PINS[i], distanceToStrength(d));
#endif
#if SERIAL_DEBUG
    Serial.print(ZONE_NAME[i]); Serial.print(F(": "));
    Serial.print(d); Serial.print(F(" cm   "));
#endif
    delay(SETTLE_MS);
  }

#if USE_BUZZER
  if (nearest < MAX_DISTANCE_CM) {
    long c = (nearest < MIN_DISTANCE_CM) ? MIN_DISTANCE_CM : nearest;
    tone(BUZZER_PIN, map(c, MIN_DISTANCE_CM, MAX_DISTANCE_CM, BUZZ_MAX_HZ, BUZZ_MIN_HZ));
  } else {
    noTone(BUZZER_PIN);
  }
#endif

#if SERIAL_DEBUG
  Serial.print(F("| loop "));
  Serial.print(millis() - t0);
  Serial.println(F(" ms"));
#endif
}
