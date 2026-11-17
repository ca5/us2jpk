#!/bin/bash
# Teensy KVM Switch Recovery Script
for dev in /dev/cu.usbmodem*; do
  if [ -e "$dev" ]; then
    python3 -c "import serial; s=serial.Serial('$dev', 115200, timeout=1); s.write(b'r'); s.close()"
    echo "Sent recovery command to $dev. KVM recovered!"
    exit 0
  fi
done
echo "Teensy usbmodem device not found."
