/*
 * EchoGuard — Ultrasonic Haptic Navigation Headband  (ESP32 version)
 * ------------------------------------------------------------
 * A headband with forward/left/right ultrasonic sensors. The closer an obstacle,
 * the stronger the vibration (or higher the buzzer pitch) the wearer feels — for
 * blind users, or first responders in smoke (ultrasonic works where cameras/IR fail).
 *
 * BOARD: ESP32 DevKit (WROOM). Install the "esp32 by Espressif Systems" core via
 *   Boards Manager (v3.x recommended — this sketch uses analogWrite() and tone(),
 *   which the v3 core provides). Select board "ESP32 Dev Module". Serial @ 115200.
 *
 * SENSOR: use an HC-SR04P / RCWL-1601 (works at 3.0–5.5 V) powered from the ESP32
 *   3V3 pin, so its ECHO output stays at 3.3 V (safe for ESP32 GPIO).
 *   Do NOT power a plain 5 V HC-SR04 from 5 V here — its 5 V ECHO can damage the
 *   ESP32. If that's all you have, add a divider on ECHO (1 kΩ to signal, 2 kΩ to GND).
 *
 * Test in STAGES with the CONFIG flags. A 5 V Arduino Uno version is kept in the
 * uno_fallback/ subfolder.
 *
 * STATUS: drafted before hardware arrived — logic complete, NOT yet run on real
 * hardware. Expect to tune the threshold/PWM constants against the Serial Monitor.
 * No external libraries required.
 * ------------------------------------------------------------
 */

// ======================= CONFIG (edit these) =======================
#define NUM_SENSORS   1      // 1 for first tests, 3 for the full headband
#define USE_BUZZER    1      // 1 = audible feedback (best for the very first test)
#define USE_MOTORS    0      // 1 = drive vibration motors via PWM
#define SERIAL_DEBUG  1      // 1 = print distances + loop time to the Serial Monitor

// distance thresholds, in cm
const int MAX_DISTANCE_CM = 100;  // start reacting at/below this distance
const int MIN_DISTANCE_CM = 8;    // at/below this = maximum intensity (clamp)

// vibration motor PWM limits (0–255; analogWrite is 8-bit by default on ESP32)
const int MOTOR_MIN_PWM = 100;    // lowest duty that still makes the motor actually spin
const int MOTOR_MAX_PWM = 180;    // cap — protects ~3 V motors when driven from the 5V/VIN rail

// passive-buzzer tone range, in Hz (use the PASSIVE buzzer for pitch feedback)
const int BUZZ_MIN_HZ = 200;
const int BUZZ_MAX_HZ = 2500;

const int FILTER_N = 4;                          // moving-average window (bigger = smoother but laggier)
const unsigned long ECHO_TIMEOUT_US = 25000UL;   // ~4 m ceiling; a 0 return means "nothing in range"
const int SETTLE_MS = 10;                        // gap between sensor pings (prevents echo crosstalk)

// ======================= PIN MAP (Freenove ESP32-WROVER-CAM) =======================
// This board has an ONBOARD CAMERA + SD-card slot that share many GPIOs, so:
//   * DETACH the camera ribbon while running EchoGuard (frees the camera GPIOs).
//   * These pins avoid the SD-card lines (2,4,12,13,14,15), flash (6-11),
//     PSRAM (16,17), and strapping pins (0,15).
// (On a plain WROOM board without a camera, 4/13/14 would also work for TRIG.)
const int  TRIG_PINS[3]  = {21, 22, 23};
const int  ECHO_PINS[3]  = {34, 35, 36};   // input-only — ideal for ECHO
const int  MOTOR_PINS[3] = {25, 26, 27};   // LEDC/analogWrite-capable
const int  BUZZER_PIN    = 33;             // free regardless of camera/SD
const char* ZONE_NAME[3] = {"FRONT", "LEFT", "RIGHT"};

// ======================= STATE =======================
long fbuf[3][FILTER_N];   // per-sensor moving-average ring buffer
int  fidx[3];

void setup() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);
    for (int k = 0; k < FILTER_N; k++) fbuf[i][k] = MAX_DISTANCE_CM;  // start "far"
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
  Serial.begin(115200);
  Serial.println("EchoGuard (ESP32) starting...");
#endif
}

// Fire one ultrasonic ping and return distance in cm, or -1 if nothing echoed back.
long readDistanceCM(int i) {
  int trig = TRIG_PINS[i], echo = ECHO_PINS[i];
  digitalWrite(trig, LOW);  delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long dur = pulseIn(echo, HIGH, ECHO_TIMEOUT_US);
  if (dur == 0) return -1;                    // no echo within range
  return (long)(dur * 0.0343 / 2.0);          // microseconds -> cm (speed of sound 343 m/s)
}

// Moving-average filter; "no echo" (-1) is treated as max distance (far).
long filtered(int i, long raw) {
  long v = (raw < 0) ? MAX_DISTANCE_CM : raw;
  fbuf[i][fidx[i]] = v;
  fidx[i] = (fidx[i] + 1) % FILTER_N;
  long sum = 0;
  for (int k = 0; k < FILTER_N; k++) sum += fbuf[i][k];
  return sum / FILTER_N;
}

// Map distance -> motor strength (0 = off, closer = stronger).
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
    Serial.print(ZONE_NAME[i]); Serial.print(": ");
    Serial.print(d); Serial.print(" cm   ");
#endif
    delay(SETTLE_MS);   // let this ping's echo fade before the next sensor
  }

#if USE_BUZZER
  // Audible proximity feedback (PASSIVE buzzer): closer -> higher pitch.
  if (nearest < MAX_DISTANCE_CM) {
    long c = (nearest < MIN_DISTANCE_CM) ? MIN_DISTANCE_CM : nearest;
    tone(BUZZER_PIN, map(c, MIN_DISTANCE_CM, MAX_DISTANCE_CM, BUZZ_MAX_HZ, BUZZ_MIN_HZ));
  } else {
    noTone(BUZZER_PIN);
  }
#endif

#if SERIAL_DEBUG
  Serial.print("| loop ");
  Serial.print(millis() - t0);   // <-- this is your sensor-to-feedback latency, for the resume
  Serial.println(" ms");
#endif
}
