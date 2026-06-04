"""Pure car-status classification.

This module is intentionally I/O-free so it can be unit-tested in isolation and
reused anywhere. It turns a decoded telemetry frame (the nested snake_case dict
produced by ``MessageToDict(preserving_proto_field_name=True)``) into one of four
high-level states, with configurable thresholds and time-based debouncing.

States (priority order, first match wins):
    MOVING   - the car is rotating wheels / motor / moving over ground.
    READY    - ready-to-drive asserted, shutdown closed, HV live, not moving.
    ON_IDLE  - HV live but not ready and not moving.
    OFF      - HV not live.

Faults are reported SEPARATELY (``active_faults`` list) and never change the
state — a fault can be active in any of the four states. ``fault_reasons`` lists
active fault conditions for display.

Per-car field differences (Orion bool contactors / lv_batt_v vs. Angelique
contactor_state / lv_v) are handled by reading several candidate field names.
"""

from __future__ import annotations

from dataclasses import dataclass, asdict, fields
from typing import Any

# Canonical state names (kept as plain strings so they serialize trivially to
# JSON / Kafka / the UI without an enum dependency).
OFF = "OFF"
ON_IDLE = "ON_IDLE"
READY = "READY"
MOVING = "MOVING"

STATES = (OFF, ON_IDLE, READY, MOVING)

# Faults are intentionally NOT a state (team decision): they are advisory and
# surfaced separately as `active_faults`, so the main state stays one of the four
# above. This keeps "what is the car doing" orthogonal to "is anything wrong".


@dataclass(frozen=True)
class Thresholds:
    """Tunable classification thresholds.

    All values are live-adjustable from the UI (see the car_status processor's
    config topic). ``from_overrides`` applies a partial dict of UI-sent values on
    top of the current thresholds, ignoring unknown / non-numeric keys so a bad
    payload can never crash the classifier.
    """

    hv_live_v: float = 50.0          # HV considered "live" at/above this pack voltage
    move_rpm: float = 50.0           # |motor rpm| at/above this => moving
    move_wheel: float = 1.0          # mean wheel speed (rad/s) at/above this => moving
    move_mps: float = 0.8            # gps speed (m/s) at/above this => moving
    min_state_ms: float = 500.0      # a new state must persist this long before commit
    max_gap_ms: float = 2000.0       # tolerated signal dropout within a segment (reserved)

    def to_dict(self) -> dict[str, float]:
        return asdict(self)

    def from_overrides(self, overrides: dict[str, Any] | None) -> "Thresholds":
        if not overrides:
            return self
        valid = {f.name for f in fields(self)}
        updates: dict[str, float] = {}
        for key, value in overrides.items():
            if key not in valid:
                continue
            try:
                updates[key] = float(value)
            except (TypeError, ValueError):
                continue
        if not updates:
            return self
        return Thresholds(**{**self.to_dict(), **updates})


# ---------------------------------------------------------------------------
# Frame field access (tolerant of per-car naming and missing tables)
# ---------------------------------------------------------------------------

def _table(frame: dict, name: str) -> dict:
    value = frame.get(name)
    return value if isinstance(value, dict) else {}


def _num(*values: Any) -> float | None:
    """First finite number among the candidates, else None."""
    for v in values:
        if v is None or isinstance(v, bool):
            continue
        try:
            f = float(v)
        except (TypeError, ValueError):
            continue
        if f == f and f not in (float("inf"), float("-inf")):  # not NaN/inf
            return f
    return None


def _truthy(*values: Any) -> bool:
    """True if any candidate is a truthy bool or a nonzero number."""
    for v in values:
        if isinstance(v, bool):
            if v:
                return True
        elif v is not None:
            n = _num(v)
            if n is not None and n != 0:
                return True
    return False


def _pick(frame: dict, table: str, *names: str) -> Any:
    t = _table(frame, table)
    for n in names:
        if n in t and t[n] is not None:
            return t[n]
    return None


# ---------------------------------------------------------------------------
# Signal extraction + per-predicate evaluation
# ---------------------------------------------------------------------------

def hv_live(frame: dict, th: Thresholds) -> bool:
    """HV is energized: pack voltage above threshold, or contactors closed."""
    pack_v = _num(_pick(frame, "pack", "hv_pack_v"))
    if pack_v is not None and pack_v >= th.hv_live_v:
        return True
    # Orion: three bool contactors. Angelique: contactor_state int (>=2 = closed).
    pos = _pick(frame, "diagnostics_high", "pos_hv_contactor")
    neg = _pick(frame, "diagnostics_high", "neg_hv_contactor")
    if _truthy(pos) and _truthy(neg):
        return True
    contactor_state = _num(_pick(frame, "pack", "contactor_state"))
    if contactor_state is not None and contactor_state >= 2:
        return True
    return False


def is_moving(frame: dict, th: Thresholds) -> bool:
    # Orion: controls.motor_speed. Angelique: dynamics.inverter_rpm.
    motor = _num(_pick(frame, "controls", "motor_speed"), _pick(frame, "dynamics", "inverter_rpm"))
    if motor is not None and abs(motor) >= th.move_rpm:
        return True

    # Orion: dynamics.fl_wheel_speed... Angelique: dynamics.flw_speed...
    wheels = [
        _num(_pick(frame, "dynamics", "fl_wheel_speed", "flw_speed")),
        _num(_pick(frame, "dynamics", "fr_wheel_speed", "frw_speed")),
        _num(_pick(frame, "dynamics", "bl_wheel_speed", "blw_speed")),
        _num(_pick(frame, "dynamics", "br_wheel_speed", "brw_speed")),
    ]
    present = [w for w in wheels if w is not None]
    if present:
        mean_wheel = sum(abs(w) for w in present) / len(present)
        if mean_wheel >= th.move_wheel:
            return True
    combined = _num(_pick(frame, "dynamics", "wheel_speed"))
    if combined is not None and abs(combined) >= th.move_wheel:
        return True

    gps = _num(_pick(frame, "dynamics", "gps_speed"), _pick(frame, "dynamics", "gps_velocity"))
    if gps is not None and abs(gps) >= th.move_mps:
        return True
    return False


def fault_reasons(frame: dict) -> list[str]:
    """List the active fault conditions (empty list => no fault)."""
    reasons: list[str] = []
    if _num(_pick(frame, "diagnostics_high", "run_faults")) not in (None, 0):
        reasons.append("run_faults")
    if _num(_pick(frame, "diagnostics_high", "post_faults")) not in (None, 0):
        reasons.append("post_faults")
    if _truthy(_pick(frame, "diagnostics_low", "imd_gnd_isolation_error")):
        reasons.append("imd_isolation")
    if _truthy(_pick(frame, "diagnostics_low", "bmb_comm_error")):
        reasons.append("bmb_comm_error")
    return reasons


def ready(frame: dict) -> bool:
    r2d = _truthy(
        _pick(frame, "diagnostics_low", "r2d_status"),
        _pick(frame, "diagnostics_low", "r2d_authorized"),
    )
    if not r2d:
        return False
    # All present shutdown legs must be closed (truthy). The schema allows up to
    # 12 legs; legs absent from a given car's frame are filtered out below.
    legs = [_pick(frame, "diagnostics_low", f"shutdown_leg{i}") for i in range(1, 13)]
    present = [v for v in legs if v is not None]
    if present and not all(_truthy(v) for v in present):
        return False
    return True


def shutdown_open_while_live(frame: dict, th: Thresholds) -> bool:
    if not hv_live(frame, th):
        return False
    legs = [_pick(frame, "diagnostics_low", f"shutdown_leg{i}") for i in range(1, 13)]
    present = [v for v in legs if v is not None]
    return bool(present) and not all(_truthy(v) for v in present)


def active_faults(frame: dict, th: Thresholds) -> list[str]:
    """All active fault conditions (advisory only — does NOT affect the state)."""
    faults = fault_reasons(frame)
    if shutdown_open_while_live(frame, th):
        faults.append("shutdown_open")
    return faults


def classify_instant(frame: dict, th: Thresholds) -> tuple[str, list[str]]:
    """Classify a single frame with no time history. Returns (state, reasons).

    Faults are deliberately not considered here — they're reported separately via
    ``active_faults``. The state reflects only what the car is *doing*.
    """
    if is_moving(frame, th):
        return MOVING, ["motion"]

    if not hv_live(frame, th):
        return OFF, ["hv_off"]

    if ready(frame):
        return READY, ["r2d", "hv_live"]

    return ON_IDLE, ["hv_live"]


# ---------------------------------------------------------------------------
# Debounced classifier (stateful wrapper around classify_instant)
# ---------------------------------------------------------------------------

@dataclass
class _Pending:
    state: str
    since_ms: float


class CarStateMachine:
    """Wraps classify_instant with hysteresis.

    A newly-observed state must persist for ``min_state_ms`` before it becomes
    the committed state, preventing flapping on noisy single frames. Thresholds
    can be swapped live via ``set_thresholds`` (the next frame uses them).
    """

    def __init__(self, thresholds: Thresholds | None = None):
        self.thresholds = thresholds or Thresholds()
        self.committed: str | None = None
        self.committed_since_ms: float | None = None
        self.last_reasons: list[str] = []
        self._pending: _Pending | None = None

    def set_thresholds(self, thresholds: Thresholds) -> None:
        self.thresholds = thresholds

    def update(self, frame: dict, t_ms: float) -> dict:
        """Feed one frame at time ``t_ms`` (epoch ms). Returns a status dict and a
        ``transition`` flag that is True only on the frame a new state commits."""
        instant, reasons = classify_instant(frame, self.thresholds)
        self.last_reasons = reasons

        if self.committed is None:
            # First frame: commit immediately so we always have a state.
            self.committed = instant
            self.committed_since_ms = t_ms
            self._pending = None
            return self._snapshot(frame, t_ms, transition=True)

        if instant == self.committed:
            self._pending = None
            return self._snapshot(frame, t_ms, transition=False)

        # instant differs from committed -> start / continue a debounce window.
        if self._pending is None or self._pending.state != instant:
            self._pending = _Pending(state=instant, since_ms=t_ms)
            return self._snapshot(frame, t_ms, transition=False)

        if t_ms - self._pending.since_ms >= self.thresholds.min_state_ms:
            self.committed = instant
            self.committed_since_ms = t_ms
            self._pending = None
            return self._snapshot(frame, t_ms, transition=True)

        return self._snapshot(frame, t_ms, transition=False)

    def _snapshot(self, frame: dict, t_ms: float, transition: bool) -> dict:
        time_in_state_ms = (
            t_ms - self.committed_since_ms if self.committed_since_ms is not None else 0.0
        )
        return {
            "state": self.committed,
            "transition": transition,
            "reasons": list(self.last_reasons),
            # Faults are advisory: reported alongside the state, never as a state.
            "active_faults": active_faults(frame, self.thresholds),
            "time_in_state_ms": time_in_state_ms,
            "hv_soc": _num(_pick(frame, "pack", "hv_soc")),
            "hv_pack_v": _num(_pick(frame, "pack", "hv_pack_v")),
            "lv_v": _num(_pick(frame, "pack", "lv_batt_v"), _pick(frame, "pack", "lv_v")),
            "lv_c": _num(_pick(frame, "pack", "lv_batt_c"), _pick(frame, "pack", "lv_c")),
            "lv_t": _num(_pick(frame, "pack", "lv_batt_t")),
            "thresholds": self.thresholds.to_dict(),
            "t_ms": t_ms,
        }
