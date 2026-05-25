import json
import logging
import math
import os
import signal
import time

import yaml
from kafka import KafkaConsumer, KafkaProducer

logging.basicConfig(level=os.getenv("LOGLEVEL", "INFO"))

CAR = os.getenv("KAFKA_CAR", "orion")
SOURCE_TOPIC = os.getenv("KAFKA_SOURCE_TOPIC", f"grafana_data_{CAR}")
OUTPUT_TOPIC = os.getenv("KAFKA_OUTPUT_TOPIC", f"grafana_data_{CAR}_derived")
CONFIG_PATH = os.getenv("ENRICHER_CONFIG", f"/app/stack/processors/field_enricher/config/{CAR}.yaml")
KAFKA_BOOTSTRAP = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")

_MATH_NS = {k: getattr(math, k) for k in dir(math) if not k.startswith("_")}
_MATH_NS.update(
    {"math": math, "abs": abs, "max": max, "min": min, "round": round, "len": len, "None": None, "True": True, "False": False}
)

_config_mtime: float = 0.0
_fields: list[dict] = []


def _load_config() -> list[dict]:
    global _config_mtime, _fields
    try:
        mtime = os.path.getmtime(CONFIG_PATH)
        if mtime == _config_mtime:
            return _fields
        with open(CONFIG_PATH) as f:
            cfg = yaml.safe_load(f)
        _fields = cfg.get("fields", [])
        _config_mtime = mtime
        logging.info("Loaded %d derived fields from %s", len(_fields), CONFIG_PATH)
    except Exception as e:
        logging.error("Failed to load config %s: %s", CONFIG_PATH, e)
    return _fields


def _enrich(msg: dict, fields: list[dict]) -> dict:
    out = dict(msg)
    ns = dict(_MATH_NS)
    for k, v in msg.items():
        if isinstance(v, (int, float, bool, list, type(None))):
            ns[k] = v
    for field in fields:
        name = field["name"]
        expr = field["expr"]
        try:
            result = eval(expr, {"__builtins__": {}}, ns)  # noqa: S307
            out[name] = result
            ns[name] = result
        except Exception as e:
            logging.debug("Field '%s' eval error: %s", name, e)
    return out


shutdown_requested = False


def _on_signal(signum, _frame):
    global shutdown_requested
    shutdown_requested = True
    logging.info("Shutdown requested (signal %s)", signum)


signal.signal(signal.SIGTERM, _on_signal)
signal.signal(signal.SIGINT, _on_signal)

logging.info("field_enricher starting. car=%s source=%s output=%s", CAR, SOURCE_TOPIC, OUTPUT_TOPIC)

consumer = KafkaConsumer(
    SOURCE_TOPIC,
    bootstrap_servers=KAFKA_BOOTSTRAP,
    group_id=f"field-enricher-{CAR}",
    auto_offset_reset="latest",
    enable_auto_commit=False,
    consumer_timeout_ms=1000,
)

producer = KafkaProducer(
    bootstrap_servers=KAFKA_BOOTSTRAP,
    value_serializer=lambda v: json.dumps(v).encode("utf-8"),
)

logging.info("Connected to Kafka. Consuming from '%s', publishing to '%s'.", SOURCE_TOPIC, OUTPUT_TOPIC)

try:
    while not shutdown_requested:
        fields = _load_config()
        batch = consumer.poll(timeout_ms=500)
        if not batch:
            continue
        for _partition, records in batch.items():
            for record in records:
                try:
                    raw = json.loads(record.value.decode("utf-8"))
                    enriched = _enrich(raw, fields)
                    producer.send(OUTPUT_TOPIC, enriched, headers=record.headers)
                except Exception as e:
                    logging.error("Error processing message: %s", e)
        consumer.commit()
    logging.info("field_enricher shutdown cleanly.")
finally:
    producer.flush()
    producer.close()
    consumer.close()
