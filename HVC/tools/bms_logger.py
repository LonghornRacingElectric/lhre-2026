#!/usr/bin/env python3
"""BMS serial logger — streams HVC output to CSV and a live cell-voltage plot.

Works in two modes automatically:
  - Pack-only: min/max lines from the Pack summary (always available)
  - Full: per-cell scatter plot when BMB lines are also present
"""
from __future__ import annotations

import re
import csv
import sys
import argparse
import threading
from datetime import datetime
from pathlib import Path

import numpy as np
import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation

TOTAL_IC     = 10
CELLS_PER_IC = 13
BAUD_RATE    = 115200

RE_PACK = re.compile(
    r'\[(?P<ts>\d+)\] \[INFO\] Pack: (?P<v>[\d.]+) V, '
    r'Tractive: (?P<tv>[\d.]+) V, '
    r'Faults\[live:(?P<fl>[^\s\]]+) latched:(?P<fla>[^\s\]]+)\], '
    r'Cell\[min:(?P<cmin>[\d.]+) max:(?P<cmax>[\d.]+) dV:(?P<cdv>[\d.]+) mV\], '
    r'Temp\[min:(?P<tmin>[\d.]+) max:(?P<tmax>[\d.]+)\], '
    r'Die\[max:(?P<dtmax>[\d.]+)\], '
    r'BMS\[resp:(?P<resp>\d+) disc:(?P<disc>\d+) uv:(?P<uv>\d+) ov:(?P<ov>\d+) ot:(?P<ot>\d+)\], '
    r'State:(?P<state>\d+) Shdn:(?P<sd>\d+) Bal:(?P<bal>\d+)'
)

RE_BMB = re.compile(
    r'\[(?P<ts>\d+)\] \[INFO\] BMB (?P<ic>\d+) \(die: (?P<die>[\d.]+)C, bal: (?P<bal>\d+)\) \|(?P<cells>.*)'
)

RE_CELL = re.compile(r'(\d+\.\d+)(\*?)')
RE_ANSI = re.compile(r'\x1b\[[0-9;]*m')

PACK_HEADER = [
    'timestamp_ms', 'pack_v', 'tractive_v',
    'fault_live', 'fault_latched',
    'cell_min_v', 'cell_max_v', 'cell_dv_mv',
    'temp_min_c', 'temp_max_c', 'die_temp_max_c',
    'bms_resp', 'bms_disc', 'bms_uv', 'bms_ov', 'bms_ot',
    'state', 'shutdown', 'bal_cnt',
]


def select_port() -> str:
    ports = serial.tools.list_ports.comports()
    if not ports:
        sys.exit("No serial ports found.")
    if len(ports) == 1:
        print(f"Using {ports[0].device} — {ports[0].description}")
        return ports[0].device
    print("Available serial ports:")
    for i, p in enumerate(ports):
        print(f"  [{i + 1}] {p.device} — {p.description}")
    while True:
        try:
            n = int(input("Select port: "))
            if 1 <= n <= len(ports):
                return ports[n - 1].device
        except (ValueError, EOFError):
            pass
        print(f"  Enter a number 1–{len(ports)}.")


class BMSLogger:
    def __init__(self, port: str, debug: bool = False, resume: bool = False):
        self._debug = debug
        self.ser = serial.Serial(port, BAUD_RATE, timeout=1)

        # Per-cell scatter (populated only when BMB lines arrive)
        self._cell_times: list[float] = []
        self._cell_volts: list[float] = []
        self._cell_bals:  list[bool]  = []

        # Pack-level min/max (always populated)
        self._pack_times:    list[float] = []
        self._pack_min_v:    list[float] = []
        self._pack_max_v:    list[float] = []
        self._pack_temp_min: list[float] = []
        self._pack_temp_max: list[float] = []

        self._lock    = threading.Lock()
        self._t0_ms: int | None = None

        # BMB accumulator (per pack cycle, reader-thread only)
        self._pending_pack: dict | None = None
        self._bmbs: dict[int, dict]    = {}

        out_dir = Path(__file__).parent / 'out'
        out_dir.mkdir(exist_ok=True)

        if resume:
            candidates = sorted(out_dir.glob('*.csv'))
            resume_path = candidates[-1] if candidates else None
        else:
            resume_path = None

        if resume_path is not None:
            self._load_existing(resume_path)
            self._csvf   = open(resume_path, 'a', newline='')
            self._writer = csv.writer(self._csvf)
            print(f"Resuming → {resume_path}")
        else:
            csv_path     = out_dir / f"{datetime.now().strftime('%Y-%m-%d_%H%M%S')}.csv"
            self._csvf   = open(csv_path, 'w', newline='')
            self._writer = csv.writer(self._csvf)
            self._writer.writerow(PACK_HEADER)
            self._csvf.flush()
            print(f"Logging → {csv_path}")

    def _load_existing(self, path: Path) -> None:
        with open(path, newline='') as f:
            reader = csv.reader(f)
            header = next(reader)
            ts_col   = header.index('timestamp_ms')
            cmin_col = header.index('cell_min_v')
            cmax_col = header.index('cell_max_v')
            tmin_col = header.index('temp_min_c')
            tmax_col = header.index('temp_max_c')
            rows = list(reader)
        if not rows:
            return
        self._t0_ms = int(rows[0][ts_col])
        for row in rows:
            t_s = (int(row[ts_col]) - self._t0_ms) / 1000.0
            self._pack_times.append(t_s)
            self._pack_min_v.append(float(row[cmin_col]))
            self._pack_max_v.append(float(row[cmax_col]))
            self._pack_temp_min.append(float(row[tmin_col]))
            self._pack_temp_max.append(float(row[tmax_col]))
        print(f"  loaded {len(rows)} existing rows")

    def _t_s(self, ts_ms: int) -> float:
        if self._t0_ms is None:
            self._t0_ms = ts_ms
        return (ts_ms - self._t0_ms) / 1000.0

    def handle_line(self, line: str) -> None:
        m = RE_PACK.search(line)
        if m:
            ts_ms = int(m.group('ts'))
            t_s   = self._t_s(ts_ms)

            # Always write a CSV row and update pack-level plot data
            p = m.groupdict()
            self._writer.writerow([
                p['ts'], p['v'], p['tv'],
                p['fl'], p['fla'],
                p['cmin'], p['cmax'], p['cdv'],
                p['tmin'], p['tmax'], p['dtmax'],
                p['resp'], p['disc'], p['uv'], p['ov'], p['ot'],
                p['state'], p['sd'], p['bal'],
            ])
            self._csvf.flush()

            with self._lock:
                self._pack_times.append(t_s)
                self._pack_min_v.append(float(m.group('cmin')))
                self._pack_max_v.append(float(m.group('cmax')))
                self._pack_temp_min.append(float(m.group('tmin')))
                self._pack_temp_max.append(float(m.group('tmax')))

            self._pending_pack = m.groupdict()
            self._bmbs = {}
            return

        m = RE_BMB.search(line)
        if not m:
            return

        ic    = int(m.group('ic'))
        ts_ms = int(m.group('ts'))
        t_s   = self._t_s(ts_ms)
        cells = [(float(v), bool(star)) for v, star in RE_CELL.findall(m.group('cells'))
                 if 2.5 <= float(v) <= 4.3]
        self._bmbs[ic] = {'die': float(m.group('die')), 'cells': cells}

        with self._lock:
            for v, b in cells:
                self._cell_times.append(t_s)
                self._cell_volts.append(v)
                self._cell_bals.append(b)

    def run(self) -> None:
        print("Streaming serial... (close the plot or Ctrl+C to stop)")
        rows = 0
        try:
            while True:
                raw = self.ser.readline()
                if not raw:
                    continue
                line = RE_ANSI.sub('', raw.decode('utf-8', errors='replace')).strip()
                if not line:
                    continue
                if self._debug:
                    print(f"RX: {repr(line)}")
                prev = rows
                self.handle_line(line)
                with self._lock:
                    rows = len(self._pack_times)
                if rows != prev:
                    print(f"  row #{rows}", flush=True)
        except KeyboardInterrupt:
            pass
        finally:
            self._csvf.close()
            self.ser.close()

    def pack_snapshot(self) -> tuple[list, list, list, list, list]:
        with self._lock:
            return (list(self._pack_times), list(self._pack_min_v), list(self._pack_max_v),
                    list(self._pack_temp_min), list(self._pack_temp_max))

    def cell_snapshot(self) -> tuple[list, list, list]:
        with self._lock:
            return list(self._cell_times), list(self._cell_volts), list(self._cell_bals)


def main() -> None:
    parser = argparse.ArgumentParser(description='BMS serial logger')
    parser.add_argument('--debug',    action='store_true', help='print every received line as repr()')
    parser.add_argument('--continue', action='store_true', dest='resume', help='append to and resume the last CSV')
    args = parser.parse_args()

    port   = select_port()
    logger = BMSLogger(port, debug=args.debug, resume=args.resume)

    threading.Thread(target=logger.run, daemon=True).start()

    fig, (ax, ax_temp) = plt.subplots(2, 1, figsize=(13, 9), sharex=True)

    ax.set_ylabel('Cell Voltage (V)')
    ax.set_title('Live Cell Voltages')
    sc_idle = ax.scatter([], [], s=2, c='steelblue',  linewidths=0, label='idle')
    sc_bal  = ax.scatter([], [], s=2, c='tomato',     linewidths=0, label='balancing')
    ln_min, = ax.plot([], [], '-', color='royalblue', lw=1.5, label='cell min')
    ln_max, = ax.plot([], [], '-', color='orangered', lw=1.5, label='cell max')
    ax.legend(loc='upper left', markerscale=4, framealpha=0.7)
    ax.grid(True, linestyle='--', alpha=0.5)

    ax_temp.set_xlabel('Time (s)')
    ax_temp.set_ylabel('Temperature (C)')
    ax_temp.set_title('Live Pack Temperatures')
    ln_tmin, = ax_temp.plot([], [], '-', color='royalblue', lw=1.5, label='temp min')
    ln_tmax, = ax_temp.plot([], [], '-', color='orangered', lw=1.5, label='temp max')
    ax_temp.legend(loc='upper left', framealpha=0.7)
    ax_temp.grid(True, linestyle='--', alpha=0.5)

    def update(_frame):
        pt, pmin, pmax, ptmin, ptmax = logger.pack_snapshot()
        ct, cv, cb     = logger.cell_snapshot()

        all_t: list[float] = []
        all_v: list[float] = []

        if ct:
            t = np.asarray(ct)
            v = np.asarray(cv)
            b = np.asarray(cb, dtype=bool)
            sc_idle.set_offsets(np.column_stack([t[~b], v[~b]]) if (~b).any() else np.empty((0, 2)))
            sc_bal.set_offsets( np.column_stack([t[b],  v[b]])  if b.any()    else np.empty((0, 2)))
            all_t.extend(ct)
            all_v.extend(cv)

        if pt:
            t_arr = np.asarray(pt)
            ln_min.set_data(t_arr, np.asarray(pmin))
            ln_max.set_data(t_arr, np.asarray(pmax))
            all_t.extend(pt)
            all_v.extend(pmin)
            all_v.extend(pmax)

            ln_tmin.set_data(t_arr, np.asarray(ptmin))
            ln_tmax.set_data(t_arr, np.asarray(ptmax))
            ax_temp.set_xlim(0, max(pt) + 1)
            ax_temp.set_ylim(min(ptmin) - 0.5, max(ptmax) + 0.5)

        if all_t:
            ax.set_xlim(0, max(all_t) + 1)
            ax.set_ylim(min(all_v) - 0.005, max(all_v) + 0.005)

    _ani = animation.FuncAnimation(fig, update, interval=500)
    plt.tight_layout()
    plt.show()


if __name__ == '__main__':
    main()
