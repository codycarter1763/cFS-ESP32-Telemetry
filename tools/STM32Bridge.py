import serial
import socket
import struct

SERIAL_PORT = "/dev/ttyUSB0"
BAUD = 115200

UDP_IP = "127.0.0.1"
UDP_PORT = 1234       # same port CI_LAB listens on

SYNC = b'\xAA\x55'

ser = serial.Serial(SERIAL_PORT, BAUD, timeout=2)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print("Bridge running...")

def find_sync():
    window = b''
    while True:
        b = ser.read(1)
        if not b:
            continue
        window = (window + b)[-2:]
        if window == SYNC:
            return

while True:
    find_sync()

    hdr = ser.read(2)
    if len(hdr) != 2:
        continue

    pkt_len = struct.unpack("<H", hdr)[0]

    if pkt_len < 8 or pkt_len > 256:
        continue

    packet = ser.read(pkt_len)
    if len(packet) != pkt_len:
        continue

    sock.sendto(packet, (UDP_IP, UDP_PORT))