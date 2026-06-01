import argparse
import csv
import importlib.util
import os
import re
from dataclasses import dataclass
from typing import Dict, List, Optional, Set, Tuple


_PROTOBUF_PATTERN = re.compile(r"([\w\.]+)(?:\[(\d+)\])?\s*\(([^)]+)\)")


@dataclass(frozen=True)
class ParsedField:
    # Name as it will appear in .proto (snake_case)
    proto_name: str
    # Protobuf scalar type (float/bool/int32/uint32/...)
    proto_type: str
    # Whether this is repeated
    repeated: bool
    # Estimated per-message weight (frequency * encoded-bytes); used to order field
    # *declarations* (and to pick ids for brand-new fields) so high-impact fields get
    # smaller, cheaper-to-encode tags. It no longer determines the tag of an existing field.
    weight: float = 0.0
    # Persistent protobuf tag for this field. Sourced (in priority order) from an inline
    # `#N` annotation in the CSV, then the previously generated .proto, then freshly
    # allocated. Once assigned it must never change — that is the whole point of this field.
    proto_id: Optional[int] = None


def _to_snake_case(name: str) -> str:
    s = name.strip()
    if not s:
        return "field"

    # Insert underscores for CamelCase / PascalCase boundaries
    # Examples: DiagnosticsLow -> Diagnostics_Low, GPSData -> GPS_Data
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", s)
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)

    # Replace common separators with underscores
    s = re.sub(r"[^A-Za-z0-9]+", "_", s)
    s = re.sub(r"_+", "_", s).strip("_")

    # Handle leading digits
    if s and s[0].isdigit():
        s = f"f_{s}"

    return s.lower()


def _infer_proto_type_from_can_model(byte_info: dict) -> str:
    # Prefer an explicit protobuf-declared type when present in the JSON model.
    proto_meta = byte_info.get("protobuf")
    if isinstance(proto_meta, dict):
        pb_type = (proto_meta.get("type") or "").lower()
        if pb_type:
            if pb_type in {"float", "double"}:
                return "float"
            if pb_type in {"bool", "boolean"}:
                return "bool"
            if pb_type in {"int8", "int16", "int32", "int"}:
                return "int32"
            if pb_type == "int64":
                return "int64"
            if pb_type in {"uint8", "uint16", "uint32", "byte", "uint"}:
                return "uint32"
            if pb_type == "uint64":
                return "uint64"
            # Unknown protobuf type: fall through to heuristic

    # Respect explicit boolean marker emitted by the CSV->JSON parser
    if byte_info.get("is_boolean") is True:
        return "bool"

    conv_type = (byte_info.get("conv_type") or "").lower()
    precision = byte_info.get("precision", 1.0)
    try:
        precision_f = float(precision)
    except (TypeError, ValueError):
        precision_f = 1.0

    # Handle CAN-level boolean-type names (accept both 'bool' and 'boolean')
    if conv_type in {"bool", "boolean"}:
        return "bool"

    if conv_type in {"float", "double"}:
        return "float"

    # Match get_app_type() behavior: scaled ints are represented as float
    if precision_f != 1.0:
        return "float"

    if conv_type == "bitfield":
        # Keep as integer container; can be expanded to bools later.
        return "uint32"

    if conv_type in {"int8", "int16", "int32"}:
        return "int32"
    if conv_type == "int64":
        return "int64"

    if conv_type in {"uint8", "uint16", "uint32", "byte"}:
        return "uint32"
    if conv_type == "uint64":
        return "uint64"

    return "float"


def _partition_for_packet(from_field: str, packet_info: str) -> str:
    """Maps a packet into a top-level message partition."""

    from_field_l = (from_field or "").strip().lower()
    info_l = (packet_info or "").strip().lower()

    # Dynamics: chassis motion-ish
    if any(x in from_field_l for x in ["undertray", "upright", "usm", "csm"]):
        return "Dynamics"
    if "gps" in info_l:
        return "Dynamics"

    # Controls: driver inputs & VCU originated controls
    if any(x in from_field_l for x in ["rack", "vcu"]):
        # Some VCU packets are still dynamics (GPS handled above)
        return "Controls"

    # Thermal: temps / cooling signals
    if any(k in info_l for k in ["temp", "temps", "cooling", "thermal"]):
        return "Thermal"

    # Pack: high-voltage / inverter-ish
    if "inverter" in from_field_l:
        # Inverter temps are already caught above by packet_info
        return "Pack"

    if "hvc" in from_field_l:
        # HVC packets are assigned to the Thermal partition by default (configurable later)
        return "Thermal"

    return "Diagnostics"

def _estimate_proto_encoded_size(byte_info: dict, proto_type: str) -> float:
    # Quick path for wire-sized protobuf primitives
    t = (proto_type or "").lower()
    if t == "float":
        return 4.0
    if t == "double":
        return 8.0
    if t == "bool":
        return 1.0

    # Use the CAN conv_type / length as a proxy for varint size
    conv = (byte_info.get("conv_type") or "").lower()
    length = int(byte_info.get("length") or 0)

    # Very small integer containers encode very compactly as varint.
    if conv in {"uint8", "int8", "byte"} or length == 1:
        return 1.0
    if conv in {"uint16", "int16"} or length == 2:
        return 2.0
    if conv in {"uint32", "int32"} or length == 4:
        # varint may be 1-5 bytes; assume ~2 bytes for typical sensor ranges
        return 2.0
    if conv in {"uint64", "int64"} or length == 8:
        return 4.0

    # Fallback: assume 2 bytes for a typical varint or 4 for unknown numeric types
    return 2.0

def _is_to_pi(to_field: str) -> bool:
    to_field = (to_field or "").strip()
    if not to_field:
        return False
    parts = [p.strip() for p in to_field.split(",") if p.strip()]
    return any(p.lower() == "pi" for p in parts)


def _partition_for_field(
    from_field: str,
    packet_info: str,
    field_name: str,
    frequency_hz: Optional[float],
) -> str:
    """Assign a single field to a partition message."""

    name = _to_snake_case(field_name)
    info_l = (packet_info or "").strip().lower()
    from_l = (from_field or "").strip().lower()

    # Strong name-based routing (closer to how template.proto is organized)
    if any(k in name for k in [
        "steer",
        "gps",
        "accel",
        "ang_rate",
        "ride_height",
        "sprung",
        "unsprung",
        "strain",
        "pushrod",
        "spring_displace",
        "wheel_speed",
        "dash_speed",
        "flw_speed",
        "frw_speed",
        "blw_speed",
        "brw_speed",
    ]):
        return "Dynamics"

    if any(k in name for k in [
        "fault",
        "faults",
        "error",
        "errors",
        "shutdown",
        "disconnect",
        "out_range",
        "mismatch",
        "implause",
        "imd",
        "bmb",
        "fuse",
        "status",
        "state",
        "state_machine",
        "switch",
        "precharge",
        "contactor",
        "contact",
        "r2_d",
        "r2d",
        "post_faults",
        "run_faults",
    ]):
        if frequency_hz is not None and frequency_hz >= 50.0:
            return "DiagnosticsHigh"
        return "DiagnosticsLow"

    if any(k in name for k in [
        "apps",
        "bpps",
        "bse",
        "brake",
        "accel_pedal",
        "torque",
        "pedal",
        "motor_speed",
        "motor_angle"
    ]):
        return "Controls"

    if any(k in name for k in [
        "hv_",
        "lv_",
        "contactor",
        "avg_cell",
        "soc",
        "pack_",
        "cells"
    ]):
        return "Pack"

    if any(k in name for k in [
        "temp",
        "cool",
        "radiator",
        "fan",
        "flow_rate",
        "ambient",
        "bus_bar",
        "precharge",
        "discharge",
        "motor_loop",
        "inverter_temp",
        "motor_temp",
    ]) or any(k in info_l for k in ["temp", "temps", "cooling", "thermal"]):
        return "Thermal"


    # Fallback to packet-level partitioning
    packet_partition = _partition_for_packet(from_l, info_l)
    if packet_partition == "Diagnostics":
        if frequency_hz is not None and frequency_hz >= 50.0:
            return "DiagnosticsHigh"
        return "DiagnosticsLow"
    return packet_partition


def _load_generate_can_json_module():
    here = os.path.dirname(__file__)
    path = os.path.join(here, "generate_can_json.py")
    spec = importlib.util.spec_from_file_location("generate_can_json", path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not load module spec from {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _field_name_from_byte_info(byte_info: dict) -> Tuple[Optional[str], bool]:
    """Returns (proto_field_name or None, repeated).
    """
    proto_meta = byte_info.get("protobuf")
    if isinstance(proto_meta, dict) and proto_meta.get("field"):
        field_name = str(proto_meta["field"]).strip()
        # keep only last segment of dotted names
        if "." in field_name:
            field_name = field_name.split(".")[-1]
        repeated = bool(proto_meta.get("repeated"))
        return (_to_snake_case(field_name), repeated)


    raw_name = str(byte_info.get("name") or "field").strip()
    # Look for the last occurrence of a protobuf-like pattern in the string
    matches = list(_PROTOBUF_PATTERN.finditer(raw_name))
    if matches:
        m = matches[-1]
        candidate_field = (m.group(1) or "").strip()
        candidate_index = m.group(2)
        repeated = bool(candidate_index)
        if candidate_field:
            return (_to_snake_case(candidate_field), repeated)

    # No explicit protobuf mapping found — signal the caller to skip this byte
    return (None, False)

def parse_can_model_to_partitions(packets: list) -> Dict[str, List[ParsedField]]:
    partitions: Dict[str, Dict[str, ParsedField]] = {}
    repeated_bases: Dict[str, Set[str]] = {}
    # cumulative weights per partition->field
    weight_map: Dict[str, Dict[str, float]] = {}
    # explicit inline ids per partition->field (first non-None wins; conflicts warned)
    id_map: Dict[str, Dict[str, int]] = {}

    for packet in packets:
        packet_info = str(packet.get("packet_name") or "").strip()
        from_list = packet.get("from") or []
        to_list = packet.get("to") or []
        frequency_hz = packet.get("frequency")
        try:
            frequency_hz_f: Optional[float] = float(frequency_hz) if frequency_hz is not None else 0.0
        except (TypeError, ValueError):
            frequency_hz_f = 0.0
        try:
            packet_quantity = int(packet.get("quantity") or 1)
            if packet_quantity <= 0:
                packet_quantity = 1
        except (TypeError, ValueError):
            packet_quantity = 1

    
        pname_l = (packet_info or "").strip().lower()
        exclude_keywords = {
            "bootloader",
            "firmware update",
            "write memory",
            "bus enable",
            "write memory data",
            # Exclude inverter administrative/parameter packets by name instead of by numeric ID
            "inverter details",
            "inverter parameter request",
            "inverter parameter response",
        }
        if any(k in pname_l for k in exclude_keywords):
            continue

        # Match prior packet-level logic: join lists to strings
        from_field = ", ".join(str(x) for x in from_list)

        for byte_info in packet.get("bytes") or []:
            proto_name, repeated = _field_name_from_byte_info(byte_info)
            # If there is no explicit protobuf field mapped for this CAN byte, skip it.
            if proto_name is None:
                continue
            proto_type = _infer_proto_type_from_can_model(byte_info)

            partition = _partition_for_field(from_field, packet_info, proto_name, frequency_hz_f)
            partitions.setdefault(partition, {})
            repeated_bases.setdefault(partition, set())
            weight_map.setdefault(partition, {})
            id_map.setdefault(partition, {})

            # Capture an explicit inline id, if present. The same proto field can be
            # declared in several CSV cells (repeated arrays, multi-packet merges); they
            # must agree. First non-None wins; a conflicting non-None id is a CSV bug.
            proto_meta = byte_info.get("protobuf")
            explicit_id = proto_meta.get("proto_id") if isinstance(proto_meta, dict) else None
            if explicit_id is not None:
                prior = id_map[partition].get(proto_name)
                if prior is not None and prior != explicit_id:
                    print(
                        f"Warning: conflicting proto ids for {partition}.{proto_name}: "
                        f"#{prior} vs #{explicit_id}; keeping #{prior}."
                    )
                else:
                    id_map[partition][proto_name] = explicit_id

            # Track repeated base names for later enforcement
            if repeated:
                repeated_bases[partition].add(proto_name)

            # Accumulate estimated encoded bytes * packet frequency * quantity (sequential IDs)
            est_size = _estimate_proto_encoded_size(byte_info, proto_type)
            contribution = (frequency_hz_f or 0.0) * est_size * packet_quantity
            weight_map[partition][proto_name] = weight_map[partition].get(proto_name, 0.0) + contribution

            # Merge type/repeated behavior if name already exists
            parsed = ParsedField(proto_name=proto_name, proto_type=proto_type, repeated=repeated, weight=0.0)
            existing = partitions[partition].get(proto_name)
            if existing is None:
                partitions[partition][proto_name] = parsed
            else:
                merged_repeated = existing.repeated or parsed.repeated
                merged_type = existing.proto_type
                if existing.proto_type != parsed.proto_type:
                    if "float" in {existing.proto_type, parsed.proto_type}:
                        merged_type = "float"
                    elif "bool" in {existing.proto_type, parsed.proto_type}:
                        merged_type = "bool"
                partitions[partition][proto_name] = ParsedField(
                    proto_name=proto_name,
                    proto_type=merged_type,
                    repeated=merged_repeated,
                    weight=0.0,
                )

    out: Dict[str, List[ParsedField]] = {}
    for partition, fields_map in partitions.items():
        fields: List[ParsedField] = []
        for name, fdef in fields_map.items():
            # Ensure repeated flag honored
            is_repeated = name in repeated_bases.get(partition, set())
            w = weight_map.get(partition, {}).get(name, 0.0)
            explicit_id = id_map.get(partition, {}).get(name)
            fields.append(ParsedField(
                proto_name=fdef.proto_name,
                proto_type=fdef.proto_type,
                repeated=is_repeated,
                weight=w,
                proto_id=explicit_id,
            ))

        # Sort by weight descending so highest-impact fields get the smallest tag numbers.
        out[partition] = sorted(fields, key=lambda f: (-f.weight, f.proto_name))

    return out


# The GPS fields are synthetic — they have no can_packets.csv row to carry an inline
# `#id`, so their ids are pinned here. This keeps their wire numbers stable even on a
# from-scratch regen (no proto to seed from). A real seed/inline id still takes priority.
_FORCED_GPS_IDS = {"gps": 1, "gps_imu": 2, "gps_speed": 3}


def _ensure_dynamics_gps_fields(fields: List[ParsedField]) -> List[ParsedField]:
    forced_names = ["gps", "gps_imu", "gps_speed"]
    existing: Dict[str, ParsedField] = {f.proto_name: f for f in fields}

    result: List[ParsedField] = []
    for name in forced_names:
        existing_field = existing.pop(name, None)
        weight = existing_field.weight if existing_field else 0.0
        proto_id = existing_field.proto_id if (existing_field and existing_field.proto_id is not None) else _FORCED_GPS_IDS[name]
        result.append(ParsedField(
            proto_name=name,
            proto_type="float",
            repeated=(name != "gps_speed"),
            weight=weight,
            proto_id=proto_id,
        ))

    result.extend([f for f in fields if f.proto_name not in forced_names])
    return result


def _emit_message(name: str, fields: List[ParsedField]) -> str:
    lines: List[str] = []
    lines.append(f"message {name} {{")
    # Emit in ascending tag order. Because existing fields keep their original ids,
    # this reproduces the historical field layout and simply appends new fields.
    for field in sorted(fields, key=lambda f: (f.proto_id if f.proto_id is not None else 1 << 30, f.proto_name)):
        if field.proto_id is None:
            raise ValueError(f"Field {name}.{field.proto_name} has no assigned proto id")
        repeated_kw = "repeated " if field.repeated else ""
        lines.append(f"    {repeated_kw}{field.proto_type} {field.proto_name} = {field.proto_id};")
    lines.append("}")
    return "\n".join(lines)


def _emit_board_status_message() -> str:
    lines: List[str] = []
    lines.append("message BoardStatus {")
    lines.append("    float csm_last_seen_s = 1;")
    lines.append("    float dui_last_seen_s = 2;")
    lines.append("    float hvc_last_seen_s = 3;")
    lines.append("    float inverter_last_seen_s = 4;")
    lines.append("    float pdu_last_seen_s = 5;")
    lines.append("    float tsm_last_seen_s = 6;")
    lines.append("    float usm_last_seen_s = 7;")
    lines.append("    float vcu_last_seen_s = 8;")
    lines.append("}")
    return "\n".join(lines)


_PARTITION_ORDER = ["Dynamics", "Controls", "Pack", "DiagnosticsHigh", "DiagnosticsLow", "Thermal"]

_PROTO_MESSAGE_RE = re.compile(r"^\s*message\s+(\w+)\s*\{")
_PROTO_FIELD_RE = re.compile(r"^\s*(?:repeated\s+)?\w+\s+(\w+)\s*=\s*(\d+)\s*;")


def parse_existing_proto_ids(path: Optional[str]) -> Dict[str, Dict[str, int]]:
    """Reads an existing .proto and returns {message_name: {field_name: tag}}.

    Used to seed ids so that regenerating preserves the field numbers of an
    already-deployed schema instead of reshuffling the wire format.
    """
    result: Dict[str, Dict[str, int]] = {}
    if not path or not os.path.exists(path):
        return result
    current: Optional[str] = None
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = _PROTO_MESSAGE_RE.match(line)
            if m:
                current = m.group(1)
                result.setdefault(current, {})
                continue
            if current is None:
                continue
            if "}" in line:
                current = None
                continue
            fm = _PROTO_FIELD_RE.match(line)
            if fm:
                result[current][fm.group(1)] = int(fm.group(2))
    return result


_TRAILING_ID_RE = re.compile(r"#\s*(\d+)\s*$")


def _unique_name_to_id(id_map: Dict[str, Dict[str, int]]) -> Dict[str, int]:
    """Flatten {msg: {field: id}} to {field: id}, dropping names that resolve to more
    than one id across messages (ambiguous → left un-annotated)."""
    seen: Dict[str, Set[int]] = {}
    for fields in id_map.values():
        for name, fid in fields.items():
            seen.setdefault(name, set()).add(fid)
    return {name: next(iter(ids)) for name, ids in seen.items() if len(ids) == 1}


def _splice_id(raw_line: str, old_cell: str, target_id: int, search: int) -> Tuple[str, int]:
    """Append ` #target_id` to the first occurrence of old_cell at/after `search` in
    raw_line. Returns (new_line, next_search_offset). No-op if not found."""
    if _TRAILING_ID_RE.search(old_cell.rstrip()):
        return raw_line, search  # already annotated
    idx = raw_line.find(old_cell, search)
    if idx == -1:
        return raw_line, search
    new_cell = old_cell.rstrip() + f" #{target_id}"
    return raw_line[:idx] + new_cell + raw_line[idx + len(old_cell):], idx + len(new_cell)


def _read_csv_text(path: str) -> Tuple[List[str], str, bool]:
    with open(path, "rb") as f:
        data = f.read()
    has_bom = data.startswith(b"\xef\xbb\xbf")
    text = data.decode("utf-8-sig")
    newline = "\r\n" if "\r\n" in text else "\n"
    return text.split(newline), newline, has_bom


def _write_csv_text(path: str, lines: List[str], newline: str, has_bom: bool) -> None:
    out = newline.join(lines)
    with open(path, "wb") as f:
        if has_bom:
            f.write(b"\xef\xbb\xbf")
        f.write(out.encode("utf-8"))


def write_back_bitfield_ids(bitfield_csv_path: str, id_map: Dict[str, Dict[str, int]]) -> int:
    """Annotate `Label; proto_field` cells in can_bitfields.csv with their assigned id.

    These bool fields have no can_packets.csv cell, so this is where their persistent id
    lives. The id is resolved by field name (bitfield bool names are globally unique).
    """
    if not os.path.exists(bitfield_csv_path):
        return 0
    name_to_id = _unique_name_to_id(id_map)
    lines, newline, has_bom = _read_csv_text(bitfield_csv_path)

    header_seen = False
    changed = 0
    out_lines: List[str] = []
    for line in lines:
        if not header_seen:
            out_lines.append(line)
            if line.strip():
                header_seen = True
            continue
        if not line.strip():
            out_lines.append(line)
            continue

        cells = next(csv.reader([line]))
        new_line = line
        search = 0
        for cell in cells:
            if ";" not in cell:
                continue
            proto_field = cell.split(";", 1)[1].strip()
            if not proto_field:
                continue
            proto_name = _to_snake_case(proto_field.split(".")[-1])
            target_id = name_to_id.get(proto_name)
            if target_id is None:
                continue
            before = new_line
            new_line, search = _splice_id(new_line, cell, target_id, search)
            if new_line != before:
                changed += 1
        out_lines.append(new_line)

    if changed:
        _write_csv_text(bitfield_csv_path, out_lines, newline, has_bom)
    return changed


def write_back_proto_ids(can_csv_path: str, id_map: Dict[str, Dict[str, int]]) -> int:
    """Persist assigned ids inline in can_packets.csv as a trailing `#N` on each
    protobuf field declaration. Idempotent: a cell already carrying its correct id is
    left untouched. Returns the number of cells annotated.

    Edits are done as targeted in-place string splices on the raw lines so the file's
    quoting and column formatting are preserved (a csv round-trip would re-quote
    everything and produce an enormous diff).
    """
    gen_json = _load_generate_can_json_module()

    lines, newline, has_bom = _read_csv_text(can_csv_path)

    col: Dict[str, int] = {}
    header_seen = False
    changed = 0
    out_lines: List[str] = []

    for line in lines:
        if not header_seen:
            out_lines.append(line)
            if line.strip():
                header = next(csv.reader([line]))
                col = {name.strip(): i for i, name in enumerate(header)}
                header_seen = True
            continue
        if not line.strip():
            out_lines.append(line)
            continue

        cells = next(csv.reader([line]))

        def _get(name: str) -> str:
            i = col.get(name)
            return cells[i] if i is not None and i < len(cells) else ""

        from_field = ", ".join(gen_json.parse_participants(_get("From")))
        packet_info = _get("Packet Info").strip()
        freq = gen_json.parse_frequency(_get("Frequency (Hz)") or "NA")
        freq_f = float(freq) if freq is not None else 0.0

        new_line = line
        search = 0
        for k in range(8):
            i = col.get(f"Data[{k}]")
            if i is None or i >= len(cells):
                continue
            cell = cells[i]
            if ";" not in cell:
                continue
            proto_part = cell.split(";", 1)[1]
            m = _PROTOBUF_PATTERN.search(proto_part)
            if not m:
                continue
            proto_name = _to_snake_case(m.group(1).split(".")[-1])
            partition = _partition_for_field(from_field, packet_info, proto_name, freq_f)
            target_id = id_map.get(partition, {}).get(proto_name)
            if target_id is None:
                continue

            before = new_line
            new_line, search = _splice_id(new_line, cell, target_id, search)
            if new_line != before:
                changed += 1

        out_lines.append(new_line)

    if changed:
        _write_csv_text(can_csv_path, out_lines, newline, has_bom)

    return changed


def _finalize_partitions(partitions: Dict[str, List[ParsedField]]) -> Dict[str, List[ParsedField]]:
    """Produce the exact field set that will be emitted (applies the forced GPS fields)."""
    final: Dict[str, List[ParsedField]] = {}
    for part in _PARTITION_ORDER:
        fields = partitions.get(part, [])
        if part == "Dynamics":
            fields = _ensure_dynamics_gps_fields(fields)
        if fields:
            final[part] = fields
    return final


def allocate_proto_ids(
    final_partitions: Dict[str, List[ParsedField]],
    existing_ids: Optional[Dict[str, Dict[str, int]]] = None,
) -> Tuple[Dict[str, List[ParsedField]], Dict[str, Dict[str, int]]]:
    """Assign a concrete, stable proto id to every field.

    Priority per field: explicit inline id > id from the previously generated proto
    (``existing_ids``) > next free integer in that message. Within a message ids are
    unique; new fields are allocated lowest-free in weight order so the highest-impact
    new signal gets the cheapest tag. Returns (partitions_with_ids, {msg: {field: id}}).
    """
    existing_ids = existing_ids or {}
    out: Dict[str, List[ParsedField]] = {}
    id_map: Dict[str, Dict[str, int]] = {}

    for part, fields in final_partitions.items():
        used: Set[int] = set()
        assigned: Dict[str, int] = {}

        # Pass 1: lock in explicit inline ids (authoritative).
        for f in fields:
            if f.proto_id is None:
                continue
            if f.proto_id in used:
                raise ValueError(
                    f"Duplicate proto id #{f.proto_id} in message {part} "
                    f"(collides at field '{f.proto_name}')"
                )
            assigned[f.proto_name] = f.proto_id
            used.add(f.proto_id)

        # Pass 2: seed un-ided fields from the previously generated proto.
        seed = existing_ids.get(part, {})
        # High-water floor: never allocate at or below the largest id the previous proto
        # ever used for this message, even if that field was removed this run. This keeps
        # a removed top field's number from being handed to a different new field.
        seed_high_water = max(seed.values()) if seed else 0
        for f in fields:
            if f.proto_name in assigned:
                continue
            sid = seed.get(f.proto_name)
            if sid is not None and sid not in used:
                assigned[f.proto_name] = sid
                used.add(sid)

        # Pass 3: allocate ids above the current high-water mark for anything still
        # unassigned. We deliberately do NOT fill gaps: a gap can be the number of a
        # field that was removed, and reusing it would let old encoded data be
        # misread as the new field. `fields` is weight-sorted (descending), so the
        # most impactful new signal gets the smallest (cheapest) new tag.
        high_water = max([seed_high_water] + list(used)) if (used or seed_high_water) else 0
        for f in fields:
            if f.proto_name in assigned:
                continue
            high_water += 1
            assigned[f.proto_name] = high_water
            used.add(high_water)

        out[part] = [
            ParsedField(
                proto_name=f.proto_name,
                proto_type=f.proto_type,
                repeated=f.repeated,
                weight=f.weight,
                proto_id=assigned[f.proto_name],
            )
            for f in fields
        ]
        id_map[part] = dict(assigned)

    return out, id_map


def generate_proto_text(
    partitions: Dict[str, List[ParsedField]],
    car_name: str,
    existing_ids: Optional[Dict[str, Dict[str, int]]] = None,
) -> Tuple[str, Dict[str, Dict[str, int]]]:
    """Render the .proto text and return it alongside the {msg: {field: id}} map.

    The id map is the source of truth that callers write back into the CSV.
    """
    final = _finalize_partitions(partitions)
    final, id_map = allocate_proto_ids(final, existing_ids)

    lines: List[str] = []
    lines.append('syntax = "proto3";')
    lines.append("package orion;")
    lines.append("")

    lines.append(f"message {car_name}SensorData {{")
    lines.append("    int64 time = 1;")
    lines.append("    int64 packet_id = 2;")

    # Match template.proto tag conventions (stable external schema expectations)
    if final.get("Dynamics"):
        lines.append("    Dynamics dynamics = 3;")
    if final.get("Controls"):
        lines.append("    Controls controls = 4;")
    if final.get("Pack"):
        lines.append("    Pack pack = 5;")
    if final.get("DiagnosticsHigh"):
        lines.append("    DiagnosticsHigh diagnostics_high = 6;")
    if final.get("DiagnosticsLow"):
        lines.append("    DiagnosticsLow diagnostics_low = 7;")
    if final.get("Thermal"):
        lines.append("    Thermal thermal = 8;")
    lines.append("    BoardStatus board_status = 9;")
    lines.append("}")
    lines.append("")

    lines.append(_emit_board_status_message())
    lines.append("")

    for part in _PARTITION_ORDER:
        fields = final.get(part)
        if not fields:
            continue
        lines.append(_emit_message(part, fields))
        lines.append("")

    return "\n".join(lines).rstrip() + "\n", id_map


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a proto3 schema from CAN packets CSV.")
    parser.add_argument("can_csv", help="Path to can_packets.csv")
    parser.add_argument("bitfield_csv", help="Path to can_bitfields.csv")
    parser.add_argument("--out", required=True, help="Output .proto file")
    parser.add_argument(
        "--no-write-back",
        action="store_true",
        help="Do not persist newly allocated ids back into the CSV.",
    )
    args = parser.parse_args()

    gen_json = _load_generate_can_json_module()
    bitfield_defs = gen_json.load_bitfield_definitions(args.bitfield_csv)
    packets = gen_json.process_csv(args.can_csv, bitfield_defs, args.bitfield_csv)

    # Seed from the previously generated proto so existing field numbers are preserved.
    existing_ids = parse_existing_proto_ids(args.out)

    partitions = parse_can_model_to_partitions(packets)
    proto_text, id_map = generate_proto_text(partitions, "Orion", existing_ids)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(proto_text)

    if not args.no_write_back:
        annotated = write_back_proto_ids(args.can_csv, id_map)
        annotated += write_back_bitfield_ids(args.bitfield_csv, id_map)
        if annotated:
            print(f"Annotated {annotated} CSV cell(s) with proto ids.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
