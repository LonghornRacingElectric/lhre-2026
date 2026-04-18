import time
import json
from pathlib import Path

from core.fmu import FMU
from core.lookup import ShockToWheel


BASE = Path(__file__).resolve().parent

INPUT = BASE / "data/input.txt"
OUTPUT = BASE / "data/output.txt"

POLL_DT = 0.02

TRACK_F = 48 * 0.0254
TRACK_R = 48 * 0.0254


# ----------------------------
# INIT
# ----------------------------
front = FMU(BASE / "fmus/FrKnCFMI.fmu", "front")
rear  = FMU(BASE / "fmus/RrKnCFMI.fmu", "rear")

lookup = ShockToWheel(BASE / "calibration/maps.npz")


# ----------------------------
# IO
# ----------------------------
def read_last():
    try:
        with open(INPUT, "r") as f:
            lines = f.read().splitlines()
        return lines[-1].strip() if lines else None
    except:
        return None


def parse(line):
    try:
        vals = [float(x) for x in line.split(",")]
        return vals if len(vals) == 4 else None
    except:
        return None


# ----------------------------
# GEOMETRY BUILDER
# ----------------------------
def build_geometry(front, rear):
    def side(prefix, fmu):
        return {
            # Wishbone (inner)
            "upperFore_i": fmu.get_vec3(f"{prefix}UpperFore_i"),
            "upperAft_i":  fmu.get_vec3(f"{prefix}UpperAft_i"),
            "lowerFore_i": fmu.get_vec3(f"{prefix}LowerFore_i"),
            "lowerAft_i":  fmu.get_vec3(f"{prefix}LowerAft_i"),

            # Upright (outer)
            "upper_o": fmu.get_vec3(f"{prefix}Upper_o"),
            "lower_o": fmu.get_vec3(f"{prefix}Lower_o"),

            # Steering
            "tie_i": fmu.get_vec3(f"{prefix}Tie_i"),
            "tie_o": fmu.get_vec3(f"{prefix}Tie_o"),

            # Wheel
            "wheelCenter": fmu.get_vec3(f"{prefix}WheelCenter"),
            "tire_ex":     fmu.get_vec3(f"{prefix}Tire_ex"),
            "tire_ey":     fmu.get_vec3(f"{prefix}Tire_ey"),

            # Contact patch
            "CP":      fmu.get_vec3(f"{prefix}CP"),
            "CPForce": fmu.get_vec3(f"{prefix}CPForce"),

            # Bellcrank / linkage
            "bellcrankPivot":   fmu.get_vec3(f"{prefix}BellcrankPivot"),
            "bellcrankPickup1": fmu.get_vec3(f"{prefix}BellcrankPickup1"),
            "bellcrankPickup2": fmu.get_vec3(f"{prefix}BellcrankPickup2"),
            "bellcrankPickup3": fmu.get_vec3(f"{prefix}BellcrankPickup3"),

            "rodMount":   fmu.get_vec3(f"{prefix}RodMount"),
            "shockMount": fmu.get_vec3(f"{prefix}ShockMount"),

            "barEnd": fmu.get_vec3(f"{prefix}BarEnd"),
            "armEnd": fmu.get_vec3(f"{prefix}ArmEnd"),
        }

    return {
        "front": {
            "left":  side("left",  front),
            "right": side("right", front),
        },
        "rear": {
            "left":  side("left",  rear),
            "right": side("right", rear),
        }
    }


# ----------------------------
# MAIN LOOP
# ----------------------------
def main():
    last = None
    print("🚀 running backend")

    while True:
        loop_start = time.perf_counter()

        line = read_last()

        if line and (last is None or line != last):
            update_start = time.perf_counter()

            last = line
            vals = parse(line)

            if vals is None:
                print("⚠️ invalid input:", line)
                continue

            L_FL, L_FR, L_RL, L_RR = vals

            # ------------------------
            # SHOCK → WHEEL (lookup)
            # ------------------------
            zFL = lookup.z("FL", L_FL)
            zFR = lookup.z("FR", L_FR)
            zRL = lookup.z("RL", L_RL)
            zRR = lookup.z("RR", L_RR)

            # ------------------------
            # AXLE COORDINATES
            # ------------------------
            hf = 0.5 * (zFL + zFR)
            rf = (zFR - zFL) / TRACK_F

            hr = 0.5 * (zRL + zRR)
            rr = (zRR - zRL) / TRACK_R

            # ------------------------
            # DRIVE FMUs (KINEMATIC)
            # ------------------------
            front.set("heaveInput", hf)
            front.set("rollInput", rf)
            front.set("steerInput", 0.0)

            rear.set("heaveInput", hr)
            rear.set("rollInput", rr)

            front.step(0.0)
            rear.step(0.0)

            # ------------------------
            # OUTPUT
            # ------------------------
            out = {
                "state": {
                    "zFL": zFL,
                    "zFR": zFR,
                    "zRL": zRL,
                    "zRR": zRR,
                    "hf": hf,
                    "rf": rf,
                    "hr": hr,
                    "rr": rr,
                },
                "geometry": build_geometry(front, rear)
            }

            with open(OUTPUT, "w") as f:
                f.write(json.dumps(out))

            update_dt = time.perf_counter() - update_start
            print(f"✅ updated | compute: {update_dt*1000:.2f} ms")

        loop_dt = time.perf_counter() - loop_start

        if loop_dt < POLL_DT:
            time.sleep(POLL_DT - loop_dt)


if __name__ == "__main__":
    main()