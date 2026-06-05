"""Unit tests for the pure car-status classifier.

Run directly (no Docker, no Kafka, no DB):
    python -m unittest telemtry.stack.processors.car_status.test_classifier
or from this directory:
    python -m unittest test_classifier
"""

import unittest

from classifier import (
    CarStateMachine,
    Thresholds,
    classify_instant,
    active_faults,
    OFF,
    ON_IDLE,
    READY,
    MOVING,
)


# --- frame builders (nested snake_case, like MessageToDict output) ----------

def frame(**tables) -> dict:
    """Build a decoded-telemetry-style frame from table=dict kwargs."""
    return dict(tables)


def hv_live_pack(**extra):
    return {"hv_pack_v": 400.0, "hv_soc": 80.0, "lv_batt_v": 13.2, **extra}


def closed_shutdown():
    return {f"shutdown_leg{i}": True for i in range(1, 5)}


class TestInstantClassification(unittest.TestCase):
    def setUp(self):
        self.th = Thresholds()

    def test_off_when_hv_not_live(self):
        f = frame(pack={"hv_pack_v": 5.0, "lv_batt_v": 13.0})
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, OFF)

    def test_on_idle_when_hv_live_not_ready(self):
        f = frame(
            pack=hv_live_pack(),
            diagnostics_low={**closed_shutdown(), "r2d_status": False},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, ON_IDLE)

    def test_ready_when_r2d_and_shutdown_closed(self):
        f = frame(
            pack=hv_live_pack(),
            diagnostics_low={**closed_shutdown(), "r2d_status": True},
        )
        state, reasons = classify_instant(f, self.th)
        self.assertEqual(state, READY)
        self.assertIn("r2d", reasons)

    def test_moving_by_motor_rpm(self):
        f = frame(
            pack=hv_live_pack(),
            controls={"motor_speed": 1500.0},
            diagnostics_low={**closed_shutdown(), "r2d_status": True},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, MOVING)

    def test_moving_by_wheel_speed(self):
        f = frame(
            pack=hv_live_pack(),
            dynamics={"fl_wheel_speed": 12.0, "fr_wheel_speed": 12.0,
                      "bl_wheel_speed": 11.0, "br_wheel_speed": 11.0},
            diagnostics_low={**closed_shutdown(), "r2d_status": True},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, MOVING)

    def test_moving_by_gps_speed(self):
        f = frame(
            pack=hv_live_pack(),
            dynamics={"gps_speed": 9.0},
            diagnostics_low={**closed_shutdown(), "r2d_status": True},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, MOVING)

    def test_moving_by_angelique_inverter_rpm(self):
        # Angelique reports motor speed as dynamics.inverter_rpm (not controls).
        f = frame(
            pack=hv_live_pack(),
            dynamics={"inverter_rpm": 1500.0},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, MOVING)

    def test_moving_by_angelique_wheel_speed(self):
        # Angelique wheel fields are flw_speed/frw_speed/blw_speed/brw_speed.
        f = frame(
            pack=hv_live_pack(),
            dynamics={"flw_speed": 12.0, "frw_speed": 12.0,
                      "blw_speed": 11.0, "brw_speed": 11.0},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, MOVING)

    def test_moving_takes_priority_over_ready(self):
        # Moving + ready -> MOVING (priority order).
        f = frame(
            pack=hv_live_pack(),
            controls={"motor_speed": 800.0},
            diagnostics_low={**closed_shutdown(), "r2d_status": True},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, MOVING)

    def test_faults_are_advisory_not_a_state(self):
        # A run_fault while ready -> state stays READY; fault is reported separately.
        f = frame(
            pack=hv_live_pack(),
            diagnostics_high={"run_faults": 4},
            diagnostics_low={**closed_shutdown(), "r2d_status": True},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, READY)
        self.assertIn("run_faults", active_faults(f, self.th))

    def test_fault_does_not_override_moving(self):
        f = frame(
            pack=hv_live_pack(),
            controls={"motor_speed": 2000.0},
            diagnostics_high={"run_faults": 1},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, MOVING)
        self.assertIn("run_faults", active_faults(f, self.th))

    def test_shutdown_open_while_live_is_advisory_fault(self):
        f = frame(
            pack=hv_live_pack(),
            diagnostics_low={"shutdown_leg1": True, "shutdown_leg2": False,
                             "shutdown_leg3": True, "shutdown_leg4": True},
        )
        state, _ = classify_instant(f, self.th)
        # HV live, not moving, not ready (shutdown open) -> ON_IDLE; fault advisory.
        self.assertEqual(state, ON_IDLE)
        self.assertIn("shutdown_open", active_faults(f, self.th))

    def test_no_faults_when_clean(self):
        f = frame(pack=hv_live_pack(), diagnostics_low=closed_shutdown())
        self.assertEqual(active_faults(f, self.th), [])

    def test_angelique_contactor_state_counts_as_hv_live(self):
        # Angelique uses contactor_state (int) instead of bool contactors.
        f = frame(
            pack={"contactor_state": 2, "hv_soc": 70.0, "lv_v": 13.0},
            diagnostics_low={**closed_shutdown(), "r2d_status": False},
        )
        state, _ = classify_instant(f, self.th)
        self.assertEqual(state, ON_IDLE)


class TestThresholds(unittest.TestCase):
    def test_move_rpm_threshold_respected(self):
        f = frame(pack=hv_live_pack(), controls={"motor_speed": 40.0})
        slow = Thresholds(move_rpm=50.0)
        fast_trigger = Thresholds(move_rpm=30.0)
        self.assertEqual(classify_instant(f, slow)[0], ON_IDLE)
        self.assertEqual(classify_instant(f, fast_trigger)[0], MOVING)

    def test_from_overrides_applies_valid_keys(self):
        th = Thresholds().from_overrides({"move_rpm": 10, "bogus": 1, "hv_live_v": "60"})
        self.assertEqual(th.move_rpm, 10.0)
        self.assertEqual(th.hv_live_v, 60.0)

    def test_from_overrides_ignores_bad_values(self):
        base = Thresholds()
        th = base.from_overrides({"move_rpm": "not-a-number"})
        self.assertEqual(th.move_rpm, base.move_rpm)

    def test_from_overrides_empty_is_identity(self):
        base = Thresholds()
        self.assertEqual(base.from_overrides(None), base)
        self.assertEqual(base.from_overrides({}), base)


class TestDebounce(unittest.TestCase):
    def test_first_frame_commits_immediately(self):
        sm = CarStateMachine()
        out = sm.update(frame(pack={"hv_pack_v": 1.0}), t_ms=0)
        self.assertEqual(out["state"], OFF)
        self.assertTrue(out["transition"])

    def test_brief_blip_does_not_commit(self):
        sm = CarStateMachine(Thresholds(min_state_ms=500))
        off = frame(pack={"hv_pack_v": 1.0})
        live = frame(pack=hv_live_pack(), diagnostics_low={"r2d_status": False})
        sm.update(off, t_ms=0)
        # A single ON_IDLE frame 100ms later should NOT flip the committed state.
        out = sm.update(live, t_ms=100)
        self.assertEqual(out["state"], OFF)
        self.assertFalse(out["transition"])

    def test_sustained_change_commits_after_min_state_ms(self):
        sm = CarStateMachine(Thresholds(min_state_ms=500))
        off = frame(pack={"hv_pack_v": 1.0})
        live = frame(pack=hv_live_pack(), diagnostics_low={"r2d_status": False})
        sm.update(off, t_ms=0)
        sm.update(live, t_ms=100)     # pending starts
        sm.update(live, t_ms=400)     # still pending (300ms < 500ms)
        out = sm.update(live, t_ms=700)  # 600ms >= 500ms -> commit
        self.assertEqual(out["state"], ON_IDLE)
        self.assertTrue(out["transition"])

    def test_time_in_state_accumulates(self):
        sm = CarStateMachine()
        off = frame(pack={"hv_pack_v": 1.0})
        sm.update(off, t_ms=1000)
        out = sm.update(off, t_ms=4000)
        self.assertEqual(out["time_in_state_ms"], 3000)

    def test_snapshot_carries_energy_fields(self):
        sm = CarStateMachine()
        out = sm.update(frame(pack=hv_live_pack()), t_ms=0)
        self.assertEqual(out["hv_soc"], 80.0)
        self.assertEqual(out["lv_v"], 13.2)
        self.assertIn("thresholds", out)

    def test_snapshot_carries_active_faults(self):
        sm = CarStateMachine()
        out = sm.update(
            frame(pack=hv_live_pack(), diagnostics_high={"run_faults": 2}),
            t_ms=0,
        )
        self.assertIn("active_faults", out)
        self.assertIn("run_faults", out["active_faults"])


if __name__ == "__main__":
    unittest.main()
