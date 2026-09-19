"""
EchoGuard serial logger — reads the ESP32/Uno Serial Monitor output, parses the
distance + loop-time lines, and reports the sensor-to-feedback latency (the number
you put on the resume) plus a running distance view.

Setup:
    pip install pyserial

Usage:
    python serial_logger.py COM5            # Windows (find the port in Device Manager / Arduino IDE)
    python serial_logger.py /dev/ttyUSB0    # Linux / macOS
    python serial_logger.py COM5 9600       # Uno fallback runs at 9600 baud (ESP32 = 115200)

The firmware prints lines like:  FRONT: 42 cm   | loop 11 ms
Ctrl+C to stop; it then prints the latency summary.
"""
import sys
import re
import statistics

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial not installed. Run:  pip install pyserial")

DEFAULT_BAUD = 115200  # ESP32; use 9600 for the Uno fallback


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: python serial_logger.py <PORT> [BAUD]   e.g. COM5 or /dev/ttyUSB0")
    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_BAUD

    ser = serial.Serial(port, baud, timeout=1)
    print(f"Reading {port} @ {baud} baud. Ctrl+C to stop and print the summary.\n")

    loops = []
    try:
        while True:
            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue
            print(line)
            m = re.search(r"loop\s+(\d+)\s*ms", line)
            if m:
                loops.append(int(m.group(1)))
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

    if loops:
        print("\n--- latency summary (sensor-to-feedback loop time) ---")
        print(f"samples : {len(loops)}")
        print(f"min     : {min(loops)} ms")
        print(f"median  : {int(statistics.median(loops))} ms   <-- use this figure on your resume")
        print(f"max     : {max(loops)} ms")
    else:
        print("\nNo 'loop N ms' lines seen — is SERIAL_DEBUG enabled and the baud rate correct?")


if __name__ == "__main__":
    main()
