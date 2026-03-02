"""
VCU Model Simulation
====================
Instantiates the VCU model via pybind11 bindings, configures it with the same
parameters used on the real firmware, and exercises it with a set of test
scenarios.
"""

import vcu_model_sim as vcu

# ── ADC constants (mirrored from app_freertos.c) ────────────────────────
FUDGE_FACTOR = 0.95
ADC_MAX_VAL = (1 << 12) - 1  # 4095
ADC_APPS_SCALE_V = 3.3
ADC_BSE_SCALE_V = 3.2837


def adc_v(raw_counts: float, scale_v: float) -> float:
    """Convert an ADC count to a voltage, matching firmware conversion."""
    return (FUDGE_FACTOR * raw_counts * scale_v) / ADC_MAX_VAL


# ── Build VcuParameters matching firmware defaults ──────────────────────
def make_default_params() -> vcu.VcuParameters:
    p = vcu.VcuParameters()

    p.brake_enable_threshold = 0.1
    p.buzzer_duration_ms = 1800

    # APPS
    p.apps.apps1_min_adc_v = adc_v(1200.0, ADC_APPS_SCALE_V)
    p.apps.apps1_max_adc_v = adc_v(750.0, ADC_APPS_SCALE_V)
    p.apps.apps2_min_adc_v = adc_v(1180.0, ADC_APPS_SCALE_V)
    p.apps.apps2_max_adc_v = adc_v(700.0, ADC_APPS_SCALE_V)
    p.apps.implaus_debounce_time_ms = 100
    p.apps.max_allowable_diff = 0.10
    p.apps.min_travel_deadzone = 0.12
    p.apps.max_travel_deadzone = 0.82
    p.apps.pedal_ema_alpha = 0.35

    # Torque map
    p.torque_map.max_torque_nm = 200.0

    # BSE
    p.bse.bse_off_psi = 30.0
    p.bse.bse_on_psi = 50.0
    p.bse.bse1_adc_at_min_psi_v = adc_v(370.0, ADC_BSE_SCALE_V)
    p.bse.bse1_adc_at_max_psi_v = adc_v(800.0, ADC_BSE_SCALE_V)
    p.bse.bse2_adc_at_min_psi_v = adc_v(370.0, ADC_BSE_SCALE_V)  # same as bse1 for now
    p.bse.bse2_adc_at_max_psi_v = adc_v(800.0, ADC_BSE_SCALE_V)
    p.bse.bse_max_psi = 1000.0
    p.bse.max_pedal_while_braking = 0.25
    p.bse.max_pedal_restore_threshold = 0.05
    p.bse.min_psi_deadzone = 0.0
    p.bse.max_psi_deadzone = 1.0
    p.bse.bse_ema_alpha = 1.0
    p.bse.brake_light_min_pct = 0.0
    p.bse.brake_light_max_pct = 0.30

    return p


# ── Helpers ─────────────────────────────────────────────────────────────
def print_outputs(tag: str, out: vcu.VcuOutputs) -> None:
    """Pretty-print a snapshot of the model outputs."""
    print(f"\n{'─' * 60}")
    print(f"  {tag}")
    print(f"{'─' * 60}")
    print(f"  APPS1 travel : {out.apps1_travel:7.3f}")
    print(f"  APPS2 travel : {out.apps2_travel:7.3f}")
    print(f"  Accel pedal  : {out.accel_pedal_travel:7.3f}")
    print(f"  Torque cmd   : {out.torque_cmd:7.2f} Nm")
    print(f"  Inverter en  : {out.inverter_enable}")
    print(f"  Brake pressed: {out.brake_pressed}")
    print(f"  Brake light %: {out.brake_light_pct:7.3f}")
    print(f"  PRNDL state  : {out.prndl_state}")
    print(f"  Buzzer active: {out.buzzer_active}")
    print(f"  BSE1 psi     : {out.bse1_psi:7.2f}")
    print(f"  BSE2 psi     : {out.bse2_psi:7.2f}")
    print(f"  BSE avg psi  : {out.bse_psi:7.2f}")
    print(f"  Pumps on     : {out.pumps_on}")
    print(f"  Rad fans %   : {out.rad_fans_pct:7.3f}")
    print(f"  Bat fans %   : {out.bat_fans_pct:7.3f}")
    # Faults
    f = out.faults
    faults_active = [
        name
        for name in (
            "apps1_under_range",
            "apps1_over_range",
            "apps2_under_range",
            "apps2_over_range",
            "apps_implaus",
            "apps_any_fault",
            "bse1_under_range",
            "bse1_over_range",
            "bse2_under_range",
            "bse2_over_range",
            "brake_latched",
            "brake_any_fault",
            "any_fault",
        )
        if getattr(f, name)
    ]
    print(f"  Faults       : {faults_active if faults_active else 'none'}")
    # Debug
    d = out.debug
    print(f"  Debug APPS Δ : {d.apps_diff:7.3f}")
    print(f"  Debug implaus: {d.apps_implaus_ms} ms")


# ── Main ────────────────────────────────────────────────────────────────
def main() -> None:
    params = make_default_params()
    ctx = vcu.VcuModelContext()
    vcu.vcu_model_init(ctx, params)

    dt_ms = 50  # 50 ms control loop, matching firmware

    # ── Scenario 1: Idle (no pedal, no brake) ───────────────────────────
    inp = vcu.VcuInputs()
    inp.apps1_raw = adc_v(1200.0, ADC_APPS_SCALE_V)  # min → 0 % travel
    inp.apps2_raw = adc_v(1180.0, ADC_APPS_SCALE_V)
    inp.bse1_raw = adc_v(370.0, ADC_BSE_SCALE_V)  # min → 0 psi
    inp.bse2_raw = adc_v(370.0, ADC_BSE_SCALE_V)
    inp.drive_switch = False
    inp.contactors_closed = False

    out = vcu.vcu_model_step(ctx, inp, dt_ms)
    print_outputs("Scenario 1 — Idle (no pedal, no brake)", out)

    # ── Scenario 2: Full throttle, in drive ──────────────────────────────
    inp.apps1_raw = adc_v(750.0, ADC_APPS_SCALE_V)  # max → 100 % travel
    inp.apps2_raw = adc_v(700.0, ADC_APPS_SCALE_V)
    inp.drive_switch = True
    inp.contactors_closed = True

    # Need to step a few times for PRNDL to transition and buzzer to run
    for i in range(40):
        out = vcu.vcu_model_step(ctx, inp, dt_ms)
    print_outputs("Scenario 2 — Full throttle, drive mode", out)

    # ── Scenario 3: Braking ──────────────────────────────────────────────
    inp.apps1_raw = adc_v(1200.0, ADC_APPS_SCALE_V)  # release throttle
    inp.apps2_raw = adc_v(1180.0, ADC_APPS_SCALE_V)
    inp.bse1_raw = adc_v(600.0, ADC_BSE_SCALE_V)  # mid-range brake
    inp.bse2_raw = adc_v(600.0, ADC_BSE_SCALE_V)

    out = vcu.vcu_model_step(ctx, inp, dt_ms)
    print_outputs("Scenario 3 — Braking (no throttle)", out)

    # ── Scenario 4: APPS implausibility (sensors disagree) ───────────────
    inp.apps1_raw = adc_v(750.0, ADC_APPS_SCALE_V)  # full throttle
    inp.apps2_raw = adc_v(1180.0, ADC_APPS_SCALE_V)  # released — huge diff
    inp.bse1_raw = adc_v(370.0, ADC_BSE_SCALE_V)
    inp.bse2_raw = adc_v(370.0, ADC_BSE_SCALE_V)

    # Step enough times to exceed the 100 ms implausibility debounce
    for i in range(5):
        out = vcu.vcu_model_step(ctx, inp, dt_ms)
    print_outputs("Scenario 4 — APPS implausibility (sensors disagree)", out)

    # ── Scenario 5: Partial throttle, in drive ───────────────────────────
    inp.apps1_raw = adc_v(975.0, ADC_APPS_SCALE_V)  # ~50 % travel
    inp.apps2_raw = adc_v(940.0, ADC_APPS_SCALE_V)
    inp.bse1_raw = adc_v(370.0, ADC_BSE_SCALE_V)
    inp.bse2_raw = adc_v(370.0, ADC_BSE_SCALE_V)

    # Step a few times to let filters settle and implaus to clear
    for i in range(10):
        out = vcu.vcu_model_step(ctx, inp, dt_ms)
    print_outputs("Scenario 5 — 50 % throttle, no brake", out)

    print()


if __name__ == "__main__":
    main()
