import serial
import socket
import struct

SERIAL_PORT = "/dev/ttyACM0"
BAUD = 115200

UDP_IP = "127.0.0.1"
UDP_PORT = 1234       # same port CI_LAB listens on

ser = serial.Serial(SERIAL_PORT, BAUD, timeout=2)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print("Bridge running...")

while True:

    # read 2-byte length prefix
    hdr = ser.read(2)

    if len(hdr) != 2:
        continue

    pkt_len = struct.unpack("<H", hdr)[0]

    if pkt_len < 8 or pkt_len > 256:
        print("Bad length:", pkt_len)
        continue

    packet = ser.read(pkt_len)

    if len(packet) != pkt_len:
        print("Short packet")
        continue

    sock.sendto(packet, (UDP_IP, UDP_PORT))
