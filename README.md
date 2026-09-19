# EchoGuard — Ultrasonic Haptic Navigation Headband

A wearable headband that senses nearby obstacles with ultrasonic sensors and warns
the wearer through **graded vibration** (or a buzzer during bring-up): the closer a
surface, the stronger the feedback. It is built for situations where cameras and
infrared fail — **blind and low-vision navigation**, and **first responders in
smoke** — because ultrasonic ranging is immune to darkness and particulate haze.

The firmware targets an **ESP32** and runs with **no external libraries**. A 5 V
**Arduino Uno** port is included as a fallback.

---

## Features

- **Three-zone sensing** (front / left / right) from HC-SR04-class ultrasonic sensors.
- **Time-multiplexed pinging** with a settle gap between sensors to prevent echo
  crosstalk.
- **Moving-average filtering** of each sensor to reject spikes and dropouts.
- **Proportional feedback:** distance is mapped to PWM vibration intensity (near =
  stronger), with a buzzer pitch mode for the first bring-up stage.
- **Staged build** via compile-time flags — one sensor + buzzer → one motor → full
  three-zone headband — so each layer is proven before the next is added.
- **On-device latency reporting:** every loop prints its sensor-to-feedback time,
  and `tools/serial_logger.py` summarizes it.

## Repository layout

| Path | Purpose |
| --- | --- |
| `EchoGuard.ino` | Primary firmware (ESP32). |
| `uno_fallback/uno_fallback.ino` | 5 V Arduino Uno port (no voltage dividers needed). |
| `tools/serial_logger.py` | Reads the serial stream, parses distances, and reports loop latency. |

---

## Hardware

| Part | Notes |
| --- | --- |
| ESP32 dev board | Any WROOM/WROVER dev module. Pin map avoids camera/SD/flash/strapping GPIOs so it also works on the Freenove ESP32-WROVER-CAM (detach the camera ribbon). |
| HC-SR04 ultrasonic sensor(s) | 1 for first tests, 3 for the full headband. The plain 5 V HC-SR04 uses a **1 kΩ/2 kΩ divider on each ECHO**; the 3.3 V **HC-SR04P** can be powered from 3V3 and skip it. |
| Motor driver | ULN2003 board, or per-motor 2N2222 transistor + 1N4007 flyback diode + 1 kΩ base resistor. A GPIO cannot drive a motor directly. |
| Coin vibration motors (~3 V) | One per zone; needed from the motor stage onward. |
| Passive buzzer | For the first bring-up stage (pitch feedback). |
| USB power bank, headband, adhesive | Wearable mounting and untethered power. |

---

## Quick start (ESP32)

1. Install the **Arduino IDE**.
2. **Tools → Board → Boards Manager**, install **"esp32 by Espressif Systems"**
   (v3.x — the sketch uses `analogWrite()` and `tone()` provided by the v3 core).
3. Install the USB-serial driver for your board's bridge chip (CP2102 on the
   Freenove WROVER) so it enumerates as a COM port.
4. Select your ESP32 board and its port. On the Freenove WROVER-CAM, **detach the
   camera ribbon** first — the camera shares GPIOs used here.
5. Open `EchoGuard.ino` and **Upload** (hold **BOOT** if flashing stalls at
   "Connecting…").
6. Open the **Serial Monitor at 115200 baud** to watch distances and loop time.

Uno fallback: open `uno_fallback/uno_fallback.ino`, board = **Arduino Uno**, Serial
at **9600**.

---

## How it works

- **`readDistanceCM(i)`** pulses TRIG for 10 µs, times the ECHO high-pulse with
  `pulseIn` (bounded by `ECHO_TIMEOUT_US`), and converts microseconds to centimetres
  via the speed of sound. It returns `-1` when nothing echoes back within range.
- **`filtered(i, raw)`** keeps a per-sensor moving average over `FILTER_N` readings;
  a "no echo" is treated as maximum distance ("far").
- **`distanceToStrength(cm)`** maps distance to a PWM duty: beyond `MAX_DISTANCE_CM`
  the motor is off, at/below `MIN_DISTANCE_CM` it is at full intensity, linear
  between.
- **`loop()`** polls each enabled sensor in turn with a `SETTLE_MS` gap (crosstalk
  guard), drives that zone's motor, and — in buzzer mode — maps the nearest obstacle
  to a tone. It prints the per-cycle loop time, which is the sensor-to-feedback
  latency.

The ESP32 and Uno builds share identical logic; only the pin numbers and the 3.3 V
vs 5 V wiring differ.
