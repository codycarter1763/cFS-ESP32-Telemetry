#!/usr/bin/env python3
"""
STM32 cFS Sensor Telemetry GUI
Displays live temperature and humidity from cFS via UDP port 2234
Includes Enable Telemetry button to send TO_LAB_ENABLE_OUTPUT to CI_LAB
Includes Activate LC button to send LC_SET_LC_STATE(ACTIVE) to CI_LAB
Displays LC actionpoint alerts (high temp / high humidity) parsed from
LC's own EVS events, carried on the standard EVS long-event MID
"""

import socket
import struct
import threading
import collections
import time
import tkinter as tk
from tkinter import ttk
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.dates
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from datetime import datetime

SENSOR_MID          = 0x0895
EVS_LONG_EVENT_MID  = 0x0808
UDP_PORT            = 2234
MAX_POINTS          = 60  # 2 minutes of data at 2s intervals

# TO_LAB command parameters
TO_CMD_MID  = 0x1880
ENABLE_FC   = 0x06
CI_PORT     = 1234

# Shared data
temps     = collections.deque(maxlen=MAX_POINTS)
humids    = collections.deque(maxlen=MAX_POINTS)
times     = collections.deque(maxlen=MAX_POINTS)
lock      = threading.Lock()
latest    = {'temp': None, 'humidity': None, 'seq': 0, 'packets': 0,
             'lc_temp_alert': False, 'lc_humid_alert': False}
running   = True


def enable_telemetry(dest_ip='127.0.0.1'):
    """
    Send TO_LAB_ENABLE_OUTPUT command directly to CI_LAB on port 1234.
    Packet layout:
      Primary header  (6 bytes) — command MID 0x1880
      Cmd secondary   (2 bytes) — function code + checksum
      Payload         (20 bytes) — destination IP as null-padded string
    """
    ip_bytes  = dest_ip.encode('utf-8')[:19].ljust(20, b'\x00')
    data_len  = 2 + len(ip_bytes) - 1   # cmd secondary + payload - 1 = 21

    primary = struct.pack('>HHH',
                          TO_CMD_MID,   # stream ID
                          0xC000,       # seq flags = standalone, count = 0
                          data_len)
    cmd_sec = struct.pack('>BB',
                          ENABLE_FC & 0x7F,  # function code
                          0x00)              # checksum placeholder

    packet = primary + cmd_sec + ip_bytes

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet, ('127.0.0.1', CI_PORT))
    sock.close()
    print(f"TO_LAB_ENABLE_OUTPUT sent → dest={dest_ip}")


def set_lc_state_active():
    """
    Send LC_SET_LC_STATE(ACTIVE) to CI_LAB on port 1234.
    LC boots in LC_STATE_DISABLED by design (a fault-monitoring app
    shouldn't start silently armed) -- this activates it so watchpoints
    actually get evaluated instead of always reading STALE.
    """
    packet = bytes.fromhex("18A4C0000005028501000000")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet, ('127.0.0.1', CI_PORT))
    sock.close()
    print("LC_SET_LC_STATE(ACTIVE) sent")


def udp_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(1.0)
    sock.bind(('', UDP_PORT))
    print(f"Listening on UDP port {UDP_PORT} for MID 0x{SENSOR_MID:04X}...")

    while running:
        try:
            data, addr = sock.recvfrom(4096)
            if len(data) < 6:
                continue
            stream_id = struct.unpack('>H', data[0:2])[0]

            if stream_id == EVS_LONG_EVENT_MID and len(data) >= 170:
                # EVS long-event telemetry: AppName (20B, offset 16),
                # EventID/EventType (u16 each, little-endian), SCID/PID
                # (u32 each, little-endian), then a 122-byte Message string
                app_name = data[16:36].split(b'\x00')[0].decode(errors='replace')
                message  = data[48:48+122].split(b'\x00')[0].decode(errors='replace')

                if app_name == "LC" and ("ESP32:" in message or "AP state change" in message):
                    with lock:
                        if "AP state change" in message:
                            if "PASS to FAIL" in message:
                                if "AP = 0" in message:
                                    latest['lc_temp_alert'] = True
                                elif "AP = 1" in message:
                                    latest['lc_humid_alert'] = True
                            elif "FAIL to PASS" in message:
                                if "AP = 0" in message:
                                    latest['lc_temp_alert'] = False
                                elif "AP = 1" in message:
                                    latest['lc_humid_alert'] = False
                        elif "high temperature" in message:
                            latest['lc_temp_alert'] = True
                        elif "high humidity" in message:
                            latest['lc_humid_alert'] = True
                continue

            if stream_id != SENSOR_MID:
                continue
            if len(data) < 32:
                continue

            temp_c    = struct.unpack('<f', data[16:20])[0]
            humidity  = struct.unpack('<f', data[20:24])[0]
            seq_num   = struct.unpack('<I', data[24:28])[0]

            with lock:
                now = datetime.now()
                temps.append(temp_c)
                humids.append(humidity)
                times.append(now)
                latest['temp']     = temp_c
                latest['humidity'] = humidity
                latest['seq']      = seq_num
                latest['packets'] += 1

        except socket.timeout:
            continue
        except Exception as e:
            print(f"Receiver error: {e}")

    sock.close()

class SensorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("STM32 cFS Sensor Telemetry")
        self.root.configure(bg='#0d1117')
        self.root.geometry("900x760")

        # ── Header ──────────────────────────────────────────────────────────
        hdr = tk.Frame(root, bg='#0d1117', pady=10)
        hdr.pack(fill='x', padx=20)

        tk.Label(hdr, text="STM32 Sensor Telemetry",
                 font=('Courier New', 18, 'bold'),
                 fg='#58a6ff', bg='#0d1117').pack(side='left')

        self.status_lbl = tk.Label(hdr, text="● Waiting...",
                                    font=('Courier New', 11),
                                    fg='#f0883e', bg='#0d1117')
        self.status_lbl.pack(side='right')

        # ── Enable Telemetry bar ─────────────────────────────────────────────
        tlm_bar = tk.Frame(root, bg='#161b22', pady=8)
        tlm_bar.pack(fill='x', padx=20, pady=(0, 8))

        tk.Label(tlm_bar, text="Dest IP:",
                 font=('Courier New', 10), fg='#8b949e',
                 bg='#161b22', padx=10).pack(side='left')

        self.tlm_ip = tk.StringVar(value='127.0.0.1')
        tk.Entry(tlm_bar, textvariable=self.tlm_ip,
                 font=('Courier New', 10), width=16,
                 bg='#0d1117', fg='#c9d1d9',
                 insertbackground='white',
                 relief='flat', bd=4).pack(side='left', padx=4)

        tk.Button(tlm_bar,
                  text="  Enable Telemetry  ",
                  font=('Courier New', 10, 'bold'),
                  fg='#0d1117', bg='#3fb950',
                  activebackground='#2ea043',
                  activeforeground='#0d1117',
                  relief='flat', padx=8, pady=4,
                  cursor='hand2',
                  command=self._enable_tlm).pack(side='left', padx=6)

        tk.Button(tlm_bar,
                  text="  Activate LC  ",
                  font=('Courier New', 10, 'bold'),
                  fg='#0d1117', bg='#58a6ff',
                  activebackground='#388bfd',
                  activeforeground='#0d1117',
                  relief='flat', padx=8, pady=4,
                  cursor='hand2',
                  command=self._activate_lc).pack(side='left', padx=6)

        self.tlm_status = tk.Label(tlm_bar, text="",
                                    font=('Courier New', 10),
                                    fg='#3fb950', bg='#161b22')
        self.tlm_status.pack(side='left', padx=6)

        # ── LC alert bar ─────────────────────────────────────────────────────
        alert_bar = tk.Frame(root, bg='#161b22', pady=6)
        alert_bar.pack(fill='x', padx=20, pady=(0, 8))

        tk.Label(alert_bar, text="LC:",
                 font=('Courier New', 10), fg='#8b949e',
                 bg='#161b22', padx=10).pack(side='left')

        self.lc_alert_lbl = tk.Label(alert_bar, text="● Nominal",
                                      font=('Courier New', 10, 'bold'),
                                      fg='#3fb950', bg='#161b22')
        self.lc_alert_lbl.pack(side='left', padx=6)

        # ── Big readout cards ────────────────────────────────────────────────
        cards = tk.Frame(root, bg='#0d1117')
        cards.pack(fill='x', padx=20, pady=(0, 8))

        self.temp_var  = tk.StringVar(value="--.-")
        self.humid_var = tk.StringVar(value="--.-")

        self._make_card(cards, "TEMPERATURE", self.temp_var, "°C", '#ff7b72')
        self._make_card(cards, "HUMIDITY",    self.humid_var, "%",  '#58a6ff')

        # ── Stats row ────────────────────────────────────────────────────────
        stats = tk.Frame(root, bg='#161b22', pady=5)
        stats.pack(fill='x', padx=20, pady=(0, 8))

        self.seq_var  = tk.StringVar(value="SEQ: --")
        self.mid_var  = tk.StringVar(value=f"MID: 0x{SENSOR_MID:04X}")
        self.port_var = tk.StringVar(value=f"PORT: {UDP_PORT}")

        for var in [self.seq_var, self.mid_var, self.port_var]:
            tk.Label(stats, textvariable=var,
                     font=('Courier New', 10),
                     fg='#8b949e', bg='#161b22', padx=16).pack(side='left')

        # ── Matplotlib plots ─────────────────────────────────────────────────
        self.fig, (self.ax_t, self.ax_h) = plt.subplots(2, 1, figsize=(9, 3.8))
        self.fig.patch.set_facecolor('#0d1117')

        for ax in (self.ax_t, self.ax_h):
            ax.set_facecolor('#161b22')
            ax.tick_params(colors='#8b949e', labelsize=8)
            for spine in ax.spines.values():
                spine.set_edgecolor('#30363d')
            ax.grid(True, color='#21262d', linewidth=0.5)

        self.ax_t.set_ylabel('Temp (°C)',     color='#ff7b72', fontsize=9)
        self.ax_h.set_ylabel('Humidity (%)',  color='#58a6ff', fontsize=9)
        self.ax_h.set_xlabel('Time',          color='#8b949e', fontsize=8)

        self.line_t, = self.ax_t.plot([], [], color='#ff7b72', linewidth=1.5)
        self.line_h, = self.ax_h.plot([], [], color='#58a6ff', linewidth=1.5)
        self.fill_t  = None
        self.fill_h  = None

        self.fig.tight_layout(pad=1.5)

        canvas = FigureCanvasTkAgg(self.fig, master=root)
        canvas.get_tk_widget().pack(fill='both', expand=True, padx=20, pady=(0, 10))
        self.canvas = canvas

        self.ani = animation.FuncAnimation(
            self.fig, self._update_plot, interval=2000, blit=False)

        self._update_labels()

    # ── Helpers ──────────────────────────────────────────────────────────────

    def _make_card(self, parent, label, var, unit, color):
        frame = tk.Frame(parent, bg='#161b22', padx=24, pady=12)
        frame.pack(side='left', expand=True, fill='both', padx=(0, 8))
        tk.Label(frame, text=label,
                 font=('Courier New', 9),
                 fg='#8b949e', bg='#161b22').pack(anchor='w')
        row = tk.Frame(frame, bg='#161b22')
        row.pack(anchor='w')
        tk.Label(row, textvariable=var,
                 font=('Courier New', 40, 'bold'),
                 fg=color, bg='#161b22').pack(side='left')
        tk.Label(row, text=unit,
                 font=('Courier New', 18),
                 fg=color, bg='#161b22', pady=14).pack(side='left', anchor='s')

    def _enable_tlm(self):
        try:
            enable_telemetry(self.tlm_ip.get())
            self.tlm_status.config(text="✓ Command sent", fg='#3fb950')
            self.root.after(3000, lambda: self.tlm_status.config(text=""))
        except Exception as e:
            self.tlm_status.config(text=f"✗ {e}", fg='#ff7b72')

    def _activate_lc(self):
        try:
            set_lc_state_active()
            self.tlm_status.config(text="✓ LC activated", fg='#3fb950')
            self.root.after(3000, lambda: self.tlm_status.config(text=""))
        except Exception as e:
            self.tlm_status.config(text=f"✗ {e}", fg='#ff7b72')

    def _update_plot(self, frame):
        with lock:
            if len(times) < 2:
                return
            xs   = list(times)
            ys_t = list(temps)
            ys_h = list(humids)

        self.line_t.set_data(xs, ys_t)
        self.line_h.set_data(xs, ys_h)

        if self.fill_t:
            self.fill_t.remove()
        if self.fill_h:
            self.fill_h.remove()

        self.fill_t = self.ax_t.fill_between(xs, ys_t, alpha=0.15, color='#ff7b72')
        self.fill_h = self.ax_h.fill_between(xs, ys_h, alpha=0.15, color='#58a6ff')

        self.ax_t.relim()
        self.ax_t.autoscale_view()
        self.ax_h.relim()
        self.ax_h.autoscale_view()

        fmt = matplotlib.dates.DateFormatter('%H:%M:%S')
        self.ax_t.xaxis.set_major_formatter(fmt)
        self.ax_h.xaxis.set_major_formatter(fmt)
        self.fig.autofmt_xdate(rotation=30, ha='right')

        self.canvas.draw()

    def _update_labels(self):
        with lock:
            t            = latest['temp']
            h            = latest['humidity']
            seq          = latest['seq']
            temp_alert   = latest['lc_temp_alert']
            humid_alert  = latest['lc_humid_alert']

        if t is not None:
            self.temp_var.set(f"{t:.1f}")
            self.humid_var.set(f"{h:.1f}")
            self.status_lbl.config(text="● Live", fg='#3fb950')
            self.seq_var.set(f"SEQ: {seq}")
        else:
            self.status_lbl.config(text="● Waiting...", fg='#f0883e')

        if temp_alert and humid_alert:
            self.lc_alert_lbl.config(text="● HIGH TEMP + HIGH HUMIDITY", fg='#ff7b72')
        elif temp_alert:
            self.lc_alert_lbl.config(text="● HIGH TEMPERATURE", fg='#ff7b72')
        elif humid_alert:
            self.lc_alert_lbl.config(text="● HIGH HUMIDITY", fg='#ff7b72')
        else:
            self.lc_alert_lbl.config(text="● Nominal", fg='#3fb950')

        self.root.after(500, self._update_labels)


def main():
    global running

    t = threading.Thread(target=udp_receiver, daemon=True)
    t.start()

    root = tk.Tk()
    SensorGUI(root)

    def on_close():
        global running
        running = False
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()


if __name__ == '__main__':
    main()