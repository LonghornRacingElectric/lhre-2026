#!/usr/bin/env python3
"""BMS serial logger — streams HVC output to CSV and a live cell-voltage scatter plot."""
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
    r'Faults\[live:(?P<fl>[^\s\]]+) latched:(?P<fla>[^\s\]]+)\], '
    r'Cell\[min:(?P<cmin>[\d.]+) max:(?P<cmax>[\d.]+) dV:(?P<cdv>[\d.]+) mV\], '
    r'Temp\[min:(?P<tmin>[\d.]+) max:(?P<tmax>[\d.]+)\], '
    r'DieTemp\[max:(?P<dtmax>[\d.]+)\], '
    r'BMS\[resp:(?P<resp>\d+) disc:(?P<disc>\d+) uv:(?P<uv>\d+) ov:(?P<ov>\d+) ot:(?P<ot>\d+)\], '
    r'State:(?P<state>\d+) Shutdown:(?P<sd>\d+) BalCnt:(?P<bal>\d+)'
)

RE_BMB = re.compile(
    r'\[\d+\] \[INFO\] BMB (?P<ic>\d+) \(die: (?P<die>[\d.]+)C, bal: (?P<bal>\d+)\) \|(?P<cells>.*)'
)

RE_CELL = re.compile(r'(\d+\.\d+)(\*?)')
RE_ANSI = re.compile(r'\x1b\[[0-9;]*m')


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

        # Snapshot accumulator — only touched from the reader thread
        self._pack_data: dict | None = None
        self._bmbs: dict[int, dict]  = {}

        # Plot data — guarded by _lock
        self._lock   = threading.Lock()
        self._times: list[float] = []
        self._volts: list[float] = []
        self._bals:  list[bool]  = []
        self._t0_ms: int | None  = None

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
            self._write_header()
            print(f"Logging → {csv_path}")

    def _load_existing(self, path: Path) -> None:
        n_cells = TOTAL_IC * CELLS_PER_IC
        with open(path, newline='') as f:
            reader = csv.reader(f)
            header = next(reader)
            ts_col  = header.index('timestamp_ms')
            v_start = header.index('cell_1_1_v')
            b_start = header.index('cell_1_1_bal')
            rows = list(reader)
        if not rows:
            return
        self._t0_ms = int(rows[0][ts_col])
        for row in rows:
            t_s = (int(row[ts_col]) - self._t0_ms) / 1000.0
            for i in range(n_cells):
                self._times.append(t_s)
                self._volts.append(float(row[v_start + i]))
                self._bals.append(bool(int(row[b_start + i])))
        print(f"  loaded {len(rows)} existing rows ({len(self._times)} points)")

    def _write_header(self) -> None:
        cell_v = [f'cell_{ic}_{c}_v'   for ic in range(1, TOTAL_IC+1) for c in range(1, CELLS_PER_IC+1)]
        cell_b = [f'cell_{ic}_{c}_bal' for ic in range(1, TOTAL_IC+1) for c in range(1, CELLS_PER_IC+1)]
        die_t  = [f'die_temp_{ic}_c'   for ic in range(1, TOTAL_IC+1)]
        self._writer.writerow([
            'timestamp_ms', 'pack_v',
            'fault_live', 'fault_latched',
            'cell_min_v', 'cell_max_v', 'cell_dv_mv',
            'temp_min_c', 'temp_max_c', 'die_temp_max_c',
            'bms_resp', 'bms_disc', 'bms_uv', 'bms_ov', 'bms_ot',
            'state', 'shutdown', 'bal_cnt',
            *cell_v, *cell_b, *die_t,
        ])
        self._csvf.flush()

    def _emit(self) -> None:
        p = self._pack_data
        cell_vs, cell_bs, die_ts = [], [], []
        for ic in range(1, TOTAL_IC + 1):
            for v, b in self._bmbs[ic]['cells']:
                cell_vs.append(v)
                cell_bs.append(1 if b else 0)
            die_ts.append(self._bmbs[ic]['die'])

        self._writer.writerow([
            p['ts'], p['v'],
            p['fl'], p['fla'],
            p['cmin'], p['cmax'], p['cdv'],
            p['tmin'], p['tmax'], p['dtmax'],
            p['resp'], p['disc'], p['uv'], p['ov'], p['ot'],
            p['state'], p['sd'], p['bal'],
            *cell_vs, *cell_bs, *die_ts,
        ])
        self._csvf.flush()

        ts_ms = int(p['ts'])
        if self._t0_ms is None:
            self._t0_ms = ts_ms
        t_s = (ts_ms - self._t0_ms) / 1000.0

        with self._lock:
            for v, b in zip(cell_vs, cell_bs):
                self._times.append(t_s)
                self._volts.append(v)
                self._bals.append(bool(b))

    def handle_line(self, line: str) -> None:
        m = RE_PACK.search(line)
        if m:
            self._pack_data = m.groupdict()
            self._bmbs = {}
            return

        if self._pack_data is None:
            return

        m = RE_BMB.search(line)
        if not m:
            return

        ic    = int(m.group('ic'))
        cells = [(float(v), bool(star)) for v, star in RE_CELL.findall(m.group('cells'))]
        self._bmbs[ic] = {'die': float(m.group('die')), 'cells': cells}

        if len(self._bmbs) == TOTAL_IC:
            self._emit()
            self._pack_data = None
            self._bmbs = {}

    def run(self) -> None:
        print("Streaming serial... (close the plot or Ctrl+C to stop)")
        snaps = 0
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
                prev_snaps = snaps
                self.handle_line(line)
                with self._lock:
                    snaps = len(self._times) // (TOTAL_IC * CELLS_PER_IC)
                if snaps != prev_snaps:
                    print(f"  snapshot #{snaps}", flush=True)
        except KeyboardInterrupt:
            pass
        finally:
            self._csvf.close()
            self.ser.close()

    def plot_snapshot(self) -> tuple[list, list, list]:
        with self._lock:
            return list(self._times), list(self._volts), list(self._bals)


def main() -> None:
    parser = argparse.ArgumentParser(description='BMS serial logger')
    parser.add_argument('--debug',    action='store_true', help='print every received line as repr()')
    parser.add_argument('--continue', action='store_true', dest='resume', help='append to and resume the last CSV')
    args = parser.parse_args()

    port   = select_port()
    logger = BMSLogger(port, debug=args.debug, resume=args.resume)

    threading.Thread(target=logger.run, daemon=True).start()

    fig, ax = plt.subplots(figsize=(13, 5))
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Cell Voltage (V)')
    ax.set_title('Live Cell Voltages')
    sc_idle = ax.scatter([], [], s=2, c='blue', linewidths=0, label='idle')
    sc_bal  = ax.scatter([], [], s=2, c='red',  linewidths=0, label='balancing')
    ax.legend(loc='upper left', markerscale=4, framealpha=0.7)
    ax.grid(True, linestyle='--', alpha=0.5)

    def update(_frame):
        times, volts, bals = logger.plot_snapshot()
        if not times:
            return
        t = np.asarray(times)
        v = np.asarray(volts)
        b = np.asarray(bals, dtype=bool)
        idle, bal = ~b, b
        sc_idle.set_offsets(np.column_stack([t[idle], v[idle]]) if idle.any() else np.empty((0, 2)))
        sc_bal.set_offsets( np.column_stack([t[bal],  v[bal]])  if bal.any()  else np.empty((0, 2)))
        ax.set_xlim(0, t[-1] + 1)
        ax.set_ylim(v.min() - 0.005, v.max() + 0.005)

    _ani = animation.FuncAnimation(fig, update, interval=500)
    plt.tight_layout()
    plt.show()


if __name__ == '__main__':
    main()
