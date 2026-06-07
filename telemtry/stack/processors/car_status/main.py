"""car_status processor.

Consumes the live ``sensor_data`` telemetry stream, classifies each frame into a
high-level car state (OFF / ON_IDLE / READY / MOVING / FAULT) using the pure
classifier, and publishes:

  * a ``car_status`` Kafka topic message on every committed state transition, plus
    a periodic heartbeat carrying the current state (so a late-joining UI gets
    state immediately), and
  * (Phase 2, not yet) state segments to the standalone ``car_status_segment`` DB
    table.

Classification thresholds are **live-tunable**: the processor also consumes a
``car_status_config`` topic; any message there is applied on top of the current
thresholds for every car's state machine, and the active thresholds are echoed
in each emitted ``car_status`` message so the UI can show the effect immediately.

Phase 1: classification + live emit only. No DB writes.
"""

import json
import logging
import os
import signal
import time

from google.protobuf.json_format import MessageToDict
from kafka import KafkaConsumer, KafkaProducer
from stack.ingest.protobuf import angelique_pb2, can_packets_pb2, template_pb2

from classifier import CarStateMachine, Thresholds

logging.basicConfig(level=os.getenv("LOGLEVEL", "INFO"))
logging.getLogger("kafka").setLevel(logging.WARNING)

CAR_NAME_MAP = {"angelique": "Angelique", "orion": "Orion", "nightwatch": "Nightwatch"}

KAFKA_BOOTSTRAP_SERVERS = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
INPUT_TOPIC = os.getenv("KAFKA_INPUT_TOPIC", "sensor_data")
CONFIG_TOPIC = os.getenv("KAFKA_CONFIG_TOPIC", "car_status_config")
OUTPUT_TOPIC = os.getenv("KAFKA_OUTPUT_TOPIC", "car_status")
CONSUMER_GROUP_ID = os.getenv("KAFKA_GROUP_ID", "car-status-group")
HEARTBEAT_MS = int(os.getenv("CAR_STATUS_HEARTBEAT_MS", "2000"))

DEFAULT_CAR = CAR_NAME_MAP.get(os.getenv("KAFKA_DEFAULT_CAR", "Orion").strip().lower(), "Orion")


def _normalize_car_name(raw_car):
    if raw_car is None:
        return None
    return CAR_NAME_MAP.get(raw_car.strip().lower())


def _car_from_headers(headers):
    if not headers:
        return None
    for key, value in headers:
        key_str = key.decode("utf-8", "ignore") if isinstance(key, bytes) else str(key)
        if key_str.lower() != "car_type":
            continue
        value_str = value.decode("utf-8", "ignore") if isinstance(value, bytes) else str(value)
        return _normalize_car_name(value_str)
    return None


def _decode(payload, car_type):
    if car_type == "Angelique":
        row = angelique_pb2.AngeliqueSensorData()
    elif car_type == "Orion":
        row = can_packets_pb2.OrionSensorData()
    else:
        row = template_pb2.SensorData()
    row.ParseFromString(payload)
    return MessageToDict(
        row,
        preserving_proto_field_name=True,
        always_print_fields_with_no_presence=True,
    )


def _frame_time_ms(frame, fallback_ms):
    """Prefer the packet's own time; fall back to wall clock."""
    t = frame.get("time")
    try:
        t = float(t)
        if t > 0:
            # Heuristic: seconds vs milliseconds.
            return t * 1000.0 if t < 1e11 else t
    except (TypeError, ValueError):
        pass
    return fallback_ms


# --- shared, live-tunable threshold state ----------------------------------
thresholds = Thresholds()
machines: dict[str, CarStateMachine] = {}


def _machine_for(car: str) -> CarStateMachine:
    sm = machines.get(car)
    if sm is None:
        sm = CarStateMachine(thresholds)
        machines[car] = sm
    return sm


def _apply_config(overrides: dict) -> None:
    global thresholds
    new_th = thresholds.from_overrides(overrides)
    if new_th == thresholds:
        return
    thresholds = new_th
    for sm in machines.values():
        sm.set_thresholds(new_th)
    logging.info("car_status thresholds updated: %s", thresholds.to_dict())


# --- segment accumulation for DB persistence -------------------------------
import db_writer  # noqa: E402


class _OpenSegment:
    """Accumulates stats for the currently-open state segment of one car."""

    __slots__ = ("car", "state", "start_time", "start_packet", "end_time",
                 "end_packet", "_hv_sum", "_hv_n", "_lv_sum", "_lv_n", "_faults")

    def __init__(self, car, state, t_ms, packet):
        self.car = car
        self.state = state
        self.start_time = t_ms
        self.start_packet = packet
        self.end_time = t_ms
        self.end_packet = packet
        self._hv_sum = 0.0
        self._hv_n = 0
        self._lv_sum = 0.0
        self._lv_n = 0
        self._faults: set[str] = set()

    def accumulate(self, snapshot, packet):
        self.end_time = snapshot.get("t_ms", self.end_time)
        if packet is not None:
            self.end_packet = packet
        hv = snapshot.get("hv_soc")
        if hv is not None:
            self._hv_sum += hv
            self._hv_n += 1
        lv = snapshot.get("lv_v")
        if lv is not None:
            self._lv_sum += lv
            self._lv_n += 1
        for fault in snapshot.get("active_faults", []) or []:
            self._faults.add(fault)

    def to_row(self):
        return {
            "car": self.car,
            "state": self.state,
            "start_time": int(self.start_time),
            "end_time": int(self.end_time),
            "start_packet": self.start_packet,
            "end_packet": self.end_packet,
            "hv_soc_avg": (self._hv_sum / self._hv_n) if self._hv_n else None,
            "lv_v_avg": (self._lv_sum / self._lv_n) if self._lv_n else None,
            "active_faults": ",".join(sorted(self._faults)) if self._faults else None,
        }


open_segments: dict[str, _OpenSegment] = {}


def _on_snapshot(car_lower: str, snapshot: dict, packet):
    """Track segment boundaries; on transition, close+persist the previous one."""
    seg = open_segments.get(car_lower)
    if snapshot["transition"] or seg is None or seg.state != snapshot["state"]:
        if seg is not None:
            db_writer.write_segment(seg.to_row())
        open_segments[car_lower] = _OpenSegment(
            car_lower, snapshot["state"], snapshot.get("t_ms"), packet
        )
    else:
        seg.accumulate(snapshot, packet)


def _flush_open_segments():
    for seg in open_segments.values():
        db_writer.write_segment(seg.to_row())
    open_segments.clear()


shutdown_requested = False


def _request_shutdown(signum, _frame):
    global shutdown_requested
    shutdown_requested = True
    logging.info("Shutdown signal received (%s); stopping car_status processor.", signum)


signal.signal(signal.SIGTERM, _request_shutdown)
signal.signal(signal.SIGINT, _request_shutdown)


logging.info("car_status: connecting to %s", KAFKA_BOOTSTRAP_SERVERS)
consumer = KafkaConsumer(
    INPUT_TOPIC,
    CONFIG_TOPIC,
    bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
    group_id=CONSUMER_GROUP_ID,
    max_poll_records=20,
    enable_auto_commit=False,
    auto_offset_reset="latest",
    consumer_timeout_ms=5000,
)
producer = KafkaProducer(
    bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
    value_serializer=lambda v: json.dumps(v).encode("utf-8"),
)
logging.info(
    "car_status ready. in=%s config=%s out=%s group=%s",
    INPUT_TOPIC, CONFIG_TOPIC, OUTPUT_TOPIC, CONSUMER_GROUP_ID,
)

last_heartbeat_ms = 0.0


def _emit(car: str, snapshot: dict, kind: str) -> None:
    msg = {
        "car": car.lower(),
        "kind": kind,  # "transition" | "heartbeat"
        "state": snapshot["state"],
        "reasons": snapshot["reasons"],
        "active_faults": snapshot.get("active_faults", []),
        "time_in_state_ms": snapshot["time_in_state_ms"],
        "hv_soc": snapshot["hv_soc"],
        "hv_pack_v": snapshot["hv_pack_v"],
        "lv_v": snapshot["lv_v"],
        "lv_c": snapshot["lv_c"],
        "lv_t": snapshot["lv_t"],
        "thresholds": snapshot["thresholds"],
        "t_ms": snapshot["t_ms"],
    }
    producer.send(OUTPUT_TOPIC, msg)


try:
    while not shutdown_requested:
        batch = consumer.poll(timeout_ms=1000)
        now_ms = time.time() * 1000.0

        if batch:
            for _partition, records in batch.items():
                for record in records:
                    if record.topic == CONFIG_TOPIC:
                        try:
                            payload = record.value.decode("utf-8") if isinstance(record.value, bytes) else record.value
                            _apply_config(json.loads(payload) if isinstance(payload, str) else payload)
                        except Exception as cfg_err:
                            logging.warning("bad car_status_config message: %s", cfg_err)
                        continue

                    # sensor_data
                    try:
                        car_type = _car_from_headers(record.headers) or DEFAULT_CAR
                        frame = _decode(record.value, car_type)
                    except Exception as decode_err:
                        logging.error("decode error: %s", decode_err)
                        continue

                    t_ms = _frame_time_ms(frame, now_ms)
                    sm = _machine_for(car_type)
                    snapshot = sm.update(frame, t_ms)
                    packet_id = frame.get("packet_id") or frame.get("packetId")
                    # Accumulate/close DB segments (no-op if persistence disabled).
                    _on_snapshot(car_type.lower(), snapshot, packet_id)
                    if snapshot["transition"]:
                        _emit(car_type, snapshot, "transition")
                        logging.info("car_status %s -> %s (%s)", car_type, snapshot["state"], snapshot["reasons"])
            consumer.commit()

        # Heartbeat: re-broadcast each car's current state on an interval so a
        # freshly-opened UI sees state without waiting for the next transition.
        if now_ms - last_heartbeat_ms >= HEARTBEAT_MS and machines:
            last_heartbeat_ms = now_ms
            for car, sm in machines.items():
                if sm.committed is None:
                    continue
                hb = {
                    "state": sm.committed,
                    "transition": False,
                    "reasons": list(sm.last_reasons),
                    "active_faults": [],  # advisory faults are only known on a fresh frame
                    "time_in_state_ms": (now_ms - sm.committed_since_ms) if sm.committed_since_ms else 0.0,
                    "hv_soc": None, "hv_pack_v": None, "lv_v": None, "lv_c": None, "lv_t": None,
                    "thresholds": sm.thresholds.to_dict(),
                    "t_ms": now_ms,
                }
                _emit(car, hb, "heartbeat")

    logging.info("car_status processor shutdown requested; leaving loop.")

except Exception as e:
    logging.error("car_status error: %s", e)

finally:
    # Persist whatever segments are still open so the last window isn't lost.
    try:
        _flush_open_segments()
    except Exception as flush_err:
        logging.warning("car_status segment flush failed: %s", flush_err)
    try:
        producer.flush()
        producer.close()
    except Exception:
        pass
    consumer.close()
    logging.info("car_status consumer closed.")
