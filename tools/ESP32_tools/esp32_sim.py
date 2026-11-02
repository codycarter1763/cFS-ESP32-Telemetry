#!/usr/bin/env python3
"""
Simulated ESP32 sensor node for testing cFS without hardware.

Creates a pseudo-terminal (a virtual serial port) and sends the same frames
the ESP32 firmware sends, every 2 seconds:

    AA 55 | len (uint16, little-endian) | 14-byte CCSDS packet (big-endian)

    CCSDS primary header:   packet ID (APID 0x096, sec hdr flag), sequence control, data length
    Secondary header:       seconds, subseconds, temperature_C * 100 (int16), humidity * 100 (uint16)

The device is also linked at /tmp/ttyESP32, so cFS can be pointed at it once with:
    sudo ln -sf /tmp/ttyESP32 /dev/ttyUSB0

Examples:
    python3 tools/esp32_sim.py                   # nominal: ~23 C, ~45 %
    python3 tools/esp32_sim.py --temp 32         # above the LC 30 C limit
    python3 tools/esp32_sim.py --humidity 65     # above the LC 60 % limit
"""
import argparse
import os
import pty
import random
import struct
import sys
import time
import tty

SYNC = b"\xAA\x55"
APID = 0x096
PERIOD_S = 2.0              # same rate as the firmware
LINK_PATH = "/tmp/ttyESP32"


def build_packet(seq, temp_c, humidity):
    packet_id = (0 << 13) | (0 << 12) | (1 << 11) | (APID & 0x07FF)  # version 0, TLM, sec hdr present
    seq_ctrl = (3 << 14) | (seq & 0x3FFF)                              # unsegmented
    data_len = 8 - 1                                                   # secondary header bytes - 1
    temp_raw = int(round(temp_c * 100))
    hum_raw = int(round(humidity * 100))
    return struct.pack(">HHHHHhH", packet_id, seq_ctrl, data_len, 0, 0, temp_raw, hum_raw)


def build_frame(packet):
    return SYNC + struct.pack("<H", len(packet)) + packet


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--temp", type=float, default=23.0, help="base temperature in C (default 23.0)")
    p.add_argument("--humidity", type=float, default=45.0, help="base relative humidity in %% (default 45.0)")
    p.add_argument("--jitter", type=float, default=0.3, help="random variation added to each reading")
    args = p.parse_args()

    master, slave = pty.openpty()
    tty.setraw(slave)  # raw mode: no echo or byte translation before cFS configures the port
    dev = os.ttyname(slave)

    if os.path.lexists(LINK_PATH):
        os.remove(LINK_PATH)
    os.symlink(dev, LINK_PATH)

    print(f"Simulated ESP32 on {dev} (linked as {LINK_PATH})")
    print("Ctrl+C to stop\n")

    seq = 0
    try:
        while True:
            temp = args.temp + random.uniform(-args.jitter, args.jitter)
            hum = args.humidity + random.uniform(-args.jitter, args.jitter)
            os.write(master, build_frame(build_packet(seq, temp, hum)))

            print(f"seq={seq:5d}  temp={temp:6.2f} C  humidity={hum:6.2f} %", flush=True)
            seq += 1
            time.sleep(PERIOD_S)
    except KeyboardInterrupt:
        pass
    finally:
        if os.path.islink(LINK_PATH):
            os.remove(LINK_PATH)
        os.close(master)
        os.close(slave)
    print(f"\nSent {seq} frames")


if __name__ == "__main__":
    sys.exit(main())