"""Persistence for car_status segments.

Writes one row to the standalone ``car_status_segment`` table (per-car DB) each
time a state segment closes. Uses the repo's schema-driven ORM + QueryBuilder
path (same as gps_classifier writing ``classifier``), so the table description is
introspected from the ORM model — no hand-written SQL.

DB access is optional/lazy: if the DB is unreachable or the sql_utils package
isn't importable (e.g. running the processor without the analysis deps), the
writer degrades to a no-op and the processor keeps classifying + emitting live.
"""

import logging
import os

# Map the processor's lowercase car id to the DBTarget car name.
_CAR_DB_NAME = {"orion": "Orion", "angelique": "Angelique", "nightwatch": "Nightwatch"}

_ENABLED = os.getenv("CAR_STATUS_PERSIST", "1") not in ("0", "false", "False")

# Lazy imports so the module loads even where analysis deps are absent.
_get_db = None
_QueryBuilder = None
_CarStatusSegment = None
_table_desc_cache: dict[str, dict] = {}
_import_ok = False


def _ensure_imports() -> bool:
    global _get_db, _QueryBuilder, _CarStatusSegment, _import_ok
    if _import_ok:
        return True
    try:
        from analysis.sql_utils.db_session import get_db
        from analysis.sql_utils.query_builder import QueryBuilder
        from analysis.sql_utils.models import CarStatusSegment
        _get_db = get_db
        _QueryBuilder = QueryBuilder
        _CarStatusSegment = CarStatusSegment
        _import_ok = True
    except Exception as exc:  # pragma: no cover - environment dependent
        logging.warning("car_status persistence disabled (sql_utils import failed): %s", exc)
        _import_ok = False
    return _import_ok


def _table_desc(car_db: str) -> dict:
    desc = _table_desc_cache.get(car_db)
    if desc is None:
        desc = _QueryBuilder(car_db).get_table_column_specs()[_CarStatusSegment.__tablename__]
        _table_desc_cache[car_db] = desc
    return desc


def write_segment(segment: dict) -> bool:
    """Persist one closed segment. Returns True on success, False if skipped/failed.

    `segment` keys: car, state, start_time, end_time, start_packet, end_packet,
    hv_soc_avg, lv_v_avg, active_faults (str).
    """
    if not _ENABLED or not _ensure_imports():
        return False
    car_db = _CAR_DB_NAME.get(str(segment.get("car", "")).lower())
    if car_db is None:
        return False
    row = {
        "car": segment.get("car"),
        "state": segment.get("state"),
        "start_time": segment.get("start_time"),
        "end_time": segment.get("end_time"),
        "start_packet": segment.get("start_packet"),
        "end_packet": segment.get("end_packet"),
        "hv_soc_avg": segment.get("hv_soc_avg"),
        "lv_v_avg": segment.get("lv_v_avg"),
        "active_faults": segment.get("active_faults"),
    }
    try:
        with _get_db(car_db) as session:
            _QueryBuilder.insert(
                session,
                _CarStatusSegment.__tablename__,
                _CarStatusSegment,
                row,
                _table_desc(car_db),
                commit=True,
            )
        return True
    except Exception as exc:
        logging.warning("car_status segment write failed (%s): %s", car_db, exc)
        return False
