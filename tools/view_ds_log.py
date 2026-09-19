#!/usr/bin/env python3
"""
DS log viewer + FM file commands.

decode  -- decodes raw CCSDS packet-stream files written by cFS's Data
           Storage (DS) app. DS just concatenates the same packets that
           flow live over the software bus, so this reuses the exact
           decode logic already proven in sensor_gui.py's udp_receiver.

list-files / delete-file -- sends live FM (File Manager) commands to a
           running cFS instance to browse or clean up onboard files
           (e.g. the .dat logs DS writes), same command-injection
           pattern as sensor_gui.py's enable_telemetry()/set_lc_state_active().
"""
import struct
import argparse
import csv
import socket
import os

SENSOR_MID         = 0x0895
EVS_LONG_EVENT_MID = 0x0808

DS_FILE_HEADER_SIZE = 140  # CFE_FS_Header_t (64) + DS_FileHeader_t (68)

FM_CMD_MID             = 0x188C
FM_DIR_LIST_TLM_MID    = 0x088C
FM_DELETE_FILE_CC      = 5
FM_GET_DIR_LIST_PKT_CC = 15
CI_PORT                = 1234

def print_dat_files(path):
    path = os.path.expanduser(path)

    if not os.path.isdir(path):
        print(f"Directory not found: {path}")
        return

    files = sorted(
        f for f in os.listdir(path)
        if f.endswith('.dat')
    )

    print(f"\n.dat files in {path}:")
    print("-" * 60)

    for i, filename in enumerate(files, 1):
        filepath = os.path.join(path, filename)
        size = os.path.getsize(filepath)

        print(f"{i:3}. {filename:<40} {size:>10} bytes")

    print("-" * 60)
    print(f"Total: {len(files)} .dat files")


def cmd_local_files(args):
    print_dat_files(args.path)

# ── Offline DS file decoding ────────────────────────────────────────────────

def iter_packets(path):
    """Walk a DS-recorded file, skipping its file header, yielding one packet at a time."""
    with open(path, 'rb') as f:
        data = f.read()

    offset = DS_FILE_HEADER_SIZE
    while offset + 6 <= len(data):
        length_field = struct.unpack('>H', data[offset+4:offset+6])[0]
        total_size = length_field + 7

        if total_size < 6 or offset + total_size > len(data):
            break

        yield data[offset:offset+total_size]
        offset += total_size


def decode(packet):
    stream_id = struct.unpack('>H', packet[0:2])[0]

    if stream_id == SENSOR_MID and len(packet) >= 32:
        temp, humidity = struct.unpack('<ff', packet[16:24])
        seq = struct.unpack('<I', packet[24:28])[0]
        return {'type': 'sensor', 'seq': seq, 'temp': temp, 'humidity': humidity}

    if stream_id == EVS_LONG_EVENT_MID and len(packet) >= 170:
        app = packet[16:36].split(b'\x00')[0].decode(errors='replace')
        msg = packet[48:48+122].split(b'\x00')[0].decode(errors='replace')
        return {'type': 'event', 'app': app, 'message': msg}

    return {'type': 'unknown', 'mid': stream_id, 'len': len(packet)}


def cmd_decode(args):
    csv_writer = None
    csv_file = None
    if args.csv:
        csv_file = open(args.csv, 'w', newline='')
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(['seq', 'temp_c', 'humidity_pct'])

    count = 0
    for packet in iter_packets(args.path):
        rec = decode(packet)
        count += 1

        if rec['type'] == 'sensor':
            line = f"[SENSOR] Seq={rec['seq']} Temp={rec['temp']:.1f}C Humidity={rec['humidity']:.1f}%"
            if csv_writer:
                csv_writer.writerow([rec['seq'], f"{rec['temp']:.2f}", f"{rec['humidity']:.2f}"])
        elif rec['type'] == 'event':
            line = f"[EVENT ] {rec['app']}: {rec['message']}"
        else:
            line = f"[?      ] mid=0x{rec['mid']:04X} len={rec['len']}"

        if args.filter and args.filter not in line:
            continue
        print(line)

    if csv_file:
        csv_file.close()
        print(f"\nWrote sensor readings to {args.csv}")

    print(f"\n{count} packets decoded from {args.path}")


# ── Live FM commands ─────────────────────────────────────────────────────────

def compute_checksum(packet_bytes):
    """Real cFE command checksum algorithm: 0xFF XORed with every byte,
    checksum field itself zeroed during computation."""
    chksum = 0xFF
    for b in packet_bytes:
        chksum ^= b
    return chksum


def _build_command(mid, function_code, payload):
    dlen = 2 + len(payload) - 1  # cmd secondary (2) + payload - 1
    primary = struct.pack('>HHH', mid, 0xC000, dlen)
    cmd_sec_placeholder = struct.pack('>BB', function_code, 0x00)
    packet = primary + cmd_sec_placeholder + payload
    checksum = compute_checksum(packet)
    cmd_sec = struct.pack('>BB', function_code, checksum)
    return primary + cmd_sec + payload


def list_files(path='/cf'):
    """Send FM_GET_DIR_LIST_PKT -- lists a directory's contents as telemetry."""
    path_bytes = path.encode('utf-8')[:63].ljust(64, b'\x00')
    payload = path_bytes + struct.pack('<IB3x', 0, 1)  # Offset=0, GetSizeTimeMode=1
    packet = _build_command(FM_CMD_MID, FM_GET_DIR_LIST_PKT_CC, payload)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet, ('127.0.0.1', CI_PORT))
    sock.close()
    print(f"FM_GET_DIR_LIST_PKT sent for {path}")
    print("(response arrives as telemetry on cFS terminal, not printed here)")


def delete_file(path):
    """Send FM_DELETE_FILE for the given onboard path."""
    path_bytes = path.encode('utf-8')[:63].ljust(64, b'\x00')
    packet = _build_command(FM_CMD_MID, FM_DELETE_FILE_CC, path_bytes)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet, ('127.0.0.1', CI_PORT))
    sock.close()
    print(f"FM_DELETE_FILE sent for {path}")


def cmd_list_files(args):
    list_files(args.path)


def cmd_delete_file(args):
    delete_file(args.path)


# ── CLI ──────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description="DS log viewer and FM file commands")
    sub = ap.add_subparsers(required=True)

    p_decode = sub.add_parser('decode', help="decode a DS-recorded .dat file")
    p_decode.add_argument('path', help="path to the .dat file")
    p_decode.add_argument('--filter', help="only show lines containing this text (e.g. 'LC' or 'high humidity')")
    p_decode.add_argument('--csv', help="also write sensor readings to this CSV path")
    p_decode.set_defaults(func=cmd_decode)

    p_list = sub.add_parser('list-files', help="send FM_GET_DIR_LIST_PKT to a running cFS instance")
    p_list.add_argument('path', nargs='?', default='/cf', help="onboard directory to list (default: /cf)")
    p_list.set_defaults(func=cmd_list_files)

    p_local = sub.add_parser(
        'local-files',
        help="print local .dat files in a cFS directory"
    )

    p_local.add_argument(
        'path',
        help="local cFS directory (e.g. ~/cFS/build/exe/cf)"
    )

    p_local.set_defaults(func=cmd_local_files)

    p_delete = sub.add_parser('delete-file', help="send FM_DELETE_FILE to a running cFS instance")
    p_delete.add_argument('path', help="onboard file path to delete (e.g. /cf/sensor_6000_1.dat)")
    p_delete.set_defaults(func=cmd_delete_file)

    args = ap.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()