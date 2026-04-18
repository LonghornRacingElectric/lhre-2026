#!/usr/bin/env python3
"""
End-to-end validator for live-viewer telemetry flow.

Validates:
1. Protobuf-schema-driven publish path (MQTT -> Kafka bridge -> viewer SSE).
2. Presence and movement of key live-viewer widget topics.
3. Payload table coverage against protobuf schema (sensor_data decode path).

Run with telemtry/.venv active while stack + viewer are running.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from collections import defaultdict
from pathlib import Path
from typing import Any

sys.path.append(str(Path(__file__).parents[2]))
from analysis.database.paho_testing import DataTester


TOPICS = [
    "sensor_data",
    "live_banner",
    "dashboard_screen",
    "driver_input_visualizer",
    "car_visualization",
    "thermal_headroom",
    "energy_budget",
    "map",
]


def path_get(payload: dict[str, Any], path: tuple[str, ...]) -> Any:
    cur: Any = payload
    for part in path:
        if not isinstance(cur, dict) or part not in cur:
            return None
        cur = cur[part]
    return cur


def to_number(value: Any) -> float | None:
    try:
        if value is None:
            return None
        out = float(value)
        if out != out:  # NaN
            return None
        if out in (float("inf"), float("-inf")):
            return None
        return out
    except (TypeError, ValueError):
        return None


def series_for_paths(events: list[dict[str, Any]], paths: list[tuple[str, ...]]) -> list[float]:
    series: list[float] = []
    for payload in events:
        for path in paths:
            num = to_number(path_get(payload, path))
            if num is not None:
                series.append(num)
                break
    return series


def varying(series: list[float], precision: int = 2) -> bool:
    if len(series) < 2:
        return False
    rounded = {round(value, precision) for value in series}
    return len(rounded) > 1


def snake_to_camel(name: str) -> str:
    parts = name.split("_")
    return parts[0] + "".join(p.title() for p in parts[1:])


def parse_sse_dump(path: Path, start_ms: int) -> dict[str, list[dict[str, Any]]]:
    by_topic: dict[str, list[dict[str, Any]]] = defaultdict(list)
    if not path.exists():
        return by_topic

    for raw_line in path.read_text(errors="ignore").splitlines():
        line = raw_line.strip()
        if not line.startswith("data: "):
            continue
        try:
            event = json.loads(line[6:])
        except json.JSONDecodeError:
            continue
        ts_raw = event.get("timestamp")
        try:
            event_ts = int(ts_raw)
        except (TypeError, ValueError):
            event_ts = 0
        if event_ts and event_ts < start_ms:
            continue
        topic = event.get("topic")
        payload_raw = event.get("payload")
        if not isinstance(topic, str):
            continue
        if isinstance(payload_raw, str):
            try:
                payload = json.loads(payload_raw)
            except json.JSONDecodeError:
                continue
        else:
            payload = payload_raw
        if isinstance(payload, dict):
            by_topic[topic].append(payload)
    return by_topic


def run_publisher(script_path: Path, car: str, rows: int, delay: float, schema_source: str) -> subprocess.CompletedProcess[str]:
    cmd = [
        sys.executable,
        str(script_path),
        "--car",
        car,
        "--profile",
        "viewer",
        "--rows",
        str(rows),
        "--delay",
        str(delay),
        "--schema-source",
        schema_source,
    ]
    env = os.environ.copy()
    env["TQDM_DISABLE"] = "1"
    return subprocess.run(cmd, capture_output=True, text=True, env=env)


def schema_sync_report(car: str) -> tuple[list[str], dict[str, list[str]]]:
    proto_desc = DataTester.get_proto_desc(target=car)
    orm_desc = DataTester.get_desc(db=True, target=car)

    missing_tables = sorted(set(proto_desc.keys()) - set(orm_desc.keys()))
    missing_fields: dict[str, list[str]] = {}
    for table in sorted(set(proto_desc.keys()) & set(orm_desc.keys())):
        miss = sorted(set(proto_desc[table].keys()) - set(orm_desc[table].keys()))
        if miss:
            missing_fields[table] = miss
    return missing_tables, missing_fields


def validate_topics(car: str, topic_data: dict[str, list[dict[str, Any]]]) -> tuple[bool, list[str]]:
    failures: list[str] = []

    for topic in TOPICS:
        if not topic_data.get(topic):
            failures.append(f"{topic}: no fresh events")

    live_banner = series_for_paths(topic_data.get("live_banner", []), [("battery",)])
    if not live_banner:
        failures.append("live_banner: battery missing")

    dashboard = topic_data.get("dashboard_screen", [])
    dashboard_battery = series_for_paths(dashboard, [("batteryPct",)])
    dashboard_speed = series_for_paths(dashboard, [("speed",), ("wheelSpeedAvg",)])
    dashboard_steer = series_for_paths(dashboard, [("steerColAngle",)])
    dashboard_throttle = series_for_paths(dashboard, [("throttlePct",)])
    dashboard_brake = series_for_paths(dashboard, [("brakePct",)])
    if not dashboard_battery:
        failures.append("dashboard_screen: batteryPct missing")
    if not (dashboard_speed or dashboard_steer or dashboard_throttle or dashboard_brake):
        failures.append("dashboard_screen: no dynamic channels present")
    if not (
        varying(dashboard_speed)
        or varying(dashboard_steer)
        or varying(dashboard_throttle)
        or varying(dashboard_brake)
    ):
        failures.append("dashboard_screen: dynamic channels not changing")

    driver = topic_data.get("driver_input_visualizer", [])
    driver_throttle = series_for_paths(driver, [("controls", "throttlePct")])
    driver_brake = series_for_paths(driver, [("controls", "brakePct")])
    driver_steer = series_for_paths(driver, [("controls", "steerColAngle"), ("controls", "steerV")])
    if not (driver_throttle and driver_brake):
        failures.append("driver_input_visualizer: throttle/brake missing")
    if not (driver_steer or driver_throttle or driver_brake):
        failures.append("driver_input_visualizer: no steering/throttle/brake values")
    if not (varying(driver_throttle) or varying(driver_brake) or varying(driver_steer)):
        failures.append("driver_input_visualizer: controls not changing")

    car_vis = topic_data.get("car_visualization", [])
    wheel_series = series_for_paths(
        car_vis,
        [
            ("dynamics", "flwSpeed"),
            ("dynamics", "frwSpeed"),
            ("dynamics", "blwSpeed"),
            ("dynamics", "brwSpeed"),
        ],
    )
    if not wheel_series:
        failures.append("car_visualization: wheel speeds missing")
    elif not varying(wheel_series):
        failures.append("car_visualization: wheel speeds not changing")

    thermal = topic_data.get("thermal_headroom", [])
    headroom = series_for_paths(thermal, [("thermalHeadroomC",)])
    hottest = series_for_paths(thermal, [("hottestTempC",)])
    if not headroom:
        failures.append("thermal_headroom: thermalHeadroomC missing")
    if not hottest:
        failures.append("thermal_headroom: hottestTempC missing")

    energy = topic_data.get("energy_budget", [])
    energy_soc = series_for_paths(energy, [("batteryPct",)])
    energy_hv_v = series_for_paths(energy, [("hvPackV",)])
    energy_hv_c = series_for_paths(energy, [("hvCurrent",)])
    if not energy_soc:
        failures.append("energy_budget: batteryPct missing")
    if not (energy_hv_v or energy_hv_c):
        failures.append("energy_budget: hvPackV/hvCurrent missing")

    map_events = topic_data.get("map", [])
    gps_points = []
    for payload in map_events:
        gps = path_get(payload, ("dynamics", "gps"))
        if isinstance(gps, list) and len(gps) >= 2:
            lat = to_number(gps[0])
            lon = to_number(gps[1])
            if lat is not None and lon is not None:
                gps_points.append((lat, lon))
    if not gps_points:
        failures.append("map: gps missing")
    elif len({(round(lat, 5), round(lon, 5)) for lat, lon in gps_points}) < 2:
        failures.append("map: gps not changing")

    sensor_rows = topic_data.get("sensor_data", [])
    proto_tables = list(DataTester.get_proto_desc(target=car).keys())
    expected_payload_keys = [snake_to_camel(table) for table in proto_tables]
    seen_keys = set()
    for payload in sensor_rows:
        for key in expected_payload_keys:
            if isinstance(payload.get(key), dict):
                seen_keys.add(key)
    missing_sensor_tables = [key for key in expected_payload_keys if key not in seen_keys]
    if missing_sensor_tables:
        failures.append(f"sensor_data: missing decoded tables {', '.join(missing_sensor_tables)}")

    return len(failures) == 0, failures


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate live-viewer end-to-end telemetry flow.")
    parser.add_argument("--base-url", default="http://127.0.0.1:3001", help="Viewer base URL.")
    parser.add_argument(
        "--car",
        choices=["Orion", "Angelique", "Both"],
        default="Both",
        help="Car stream(s) to validate.",
    )
    parser.add_argument("--rows", type=int, default=30, help="Rows to publish per car.")
    parser.add_argument("--delay", type=float, default=0.03, help="Publish delay used by paho_testing.")
    parser.add_argument("--capture-seconds", type=int, default=18, help="SSE capture duration.")
    parser.add_argument(
        "--schema-source",
        choices=["proto", "orm"],
        default="proto",
        help="Schema source used by paho_testing publish path.",
    )
    parser.add_argument(
        "--strict-orm-sync",
        action="store_true",
        help="Fail if ORM schema differs from protobuf schema.",
    )
    args = parser.parse_args()

    curl_path = shutil.which("curl")
    timeout_path = shutil.which("timeout")
    if not curl_path or not timeout_path:
        print("ERROR: Required commands 'curl' and 'timeout' must be available.")
        return 2

    cars = ["Orion", "Angelique"] if args.car == "Both" else [args.car]
    script_path = Path(__file__).parent / "paho_testing.py"
    if not script_path.exists():
        print(f"ERROR: Missing publisher script: {script_path}")
        return 2

    overall_failures: list[str] = []
    orm_sync_failures: list[str] = []

    print(f"Validating live-viewer pipeline via {args.base_url}")
    print(f"Cars: {', '.join(cars)} | rows/car={args.rows} | delay={args.delay} | schema_source={args.schema_source}")

    for car in cars:
        try:
            missing_tables, missing_fields = schema_sync_report(car)
            if missing_tables or missing_fields:
                details = []
                if missing_tables:
                    details.append(f"missing tables: {', '.join(missing_tables)}")
                if missing_fields:
                    field_parts = [f"{table}({len(fields)})" for table, fields in missing_fields.items()]
                    details.append(f"missing fields: {', '.join(field_parts)}")
                sync_msg = f"{car} ORM schema drift -> " + "; ".join(details)
                print(f"[WARN] {sync_msg}")
                orm_sync_failures.append(sync_msg)
            else:
                print(f"[OK] {car} ORM schema matches protobuf tables/fields.")
        except Exception as exc:
            sync_msg = f"{car} ORM schema check unavailable: {type(exc).__name__}: {exc}"
            print(f"[WARN] {sync_msg}")
            orm_sync_failures.append(sync_msg)

        with tempfile.NamedTemporaryFile(prefix=f"live_viewer_{car.lower()}_", suffix=".sse", delete=False) as tmp:
            dump_path = Path(tmp.name)

        car_query = car.lower()
        topics_query = ",".join(TOPICS)
        stream_url = f"{args.base_url.rstrip('/')}/api/kafka-stream?topics={topics_query}&car={car_query}"

        start_ms = int(time.time() * 1000)
        with dump_path.open("w") as dump_file:
            capture_proc = subprocess.Popen(
                [timeout_path, f"{args.capture_seconds}s", curl_path, "-sN", stream_url],
                stdout=dump_file,
                stderr=subprocess.DEVNULL,
            )
        time.sleep(2)

        publish_result = run_publisher(
            script_path=script_path,
            car=car,
            rows=args.rows,
            delay=args.delay,
            schema_source=args.schema_source,
        )
        capture_return = capture_proc.wait()

        if publish_result.returncode != 0:
            overall_failures.append(
                f"{car}: publisher exited {publish_result.returncode}\n{publish_result.stdout}\n{publish_result.stderr}"
            )
            try:
                dump_path.unlink()
            except OSError:
                pass
            continue
        if capture_return not in (0, 124):
            overall_failures.append(f"{car}: SSE capture exited {capture_return}")

        topic_data = parse_sse_dump(path=dump_path, start_ms=start_ms)
        try:
            dump_path.unlink()
        except OSError:
            pass

        ok, failures = validate_topics(car=car, topic_data=topic_data)
        if ok:
            counts = {topic: len(topic_data.get(topic, [])) for topic in TOPICS}
            print(f"[OK] {car} live topics healthy: {counts}")
        else:
            print(f"[FAIL] {car} live topic checks failed:")
            for failure in failures:
                print(f"  - {failure}")
            overall_failures.extend([f"{car}: {failure}" for failure in failures])

    if args.strict_orm_sync and orm_sync_failures:
        overall_failures.extend(orm_sync_failures)

    if overall_failures:
        print("\nValidation failed.")
        for failure in overall_failures:
            print(f"- {failure}")
        return 1

    print("\nValidation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
