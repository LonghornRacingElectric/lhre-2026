import argparse
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
    # Estimated per-message weight (frequency * encoded-bytes); used to assign low tags to
    # high-impact fields so their protobuf field key encodes in fewer bytes.
    weight: float = 0.0


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
        # Default HVC to Thermal for now per your request (editable later)
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
    ]):
        return "Controls"

    if any(k in name for k in [
        "hv_",
        "lv_",
        "contactor",
        "avg_cell",
        "soc",
        "pack_",
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
            fields.append(ParsedField(proto_name=fdef.proto_name, proto_type=fdef.proto_type, repeated=is_repeated, weight=w))

        # Sort by weight descending so highest-impact fields get the smallest tag numbers.
        out[partition] = sorted(fields, key=lambda f: (-f.weight, f.proto_name))

    return out

def _emit_message(name: str, fields: List[ParsedField]) -> str:
    lines: List[str] = []
    lines.append(f"message {name} {{")
    tag = 1
    for field in fields:
        repeated_kw = "repeated " if field.repeated else ""
        lines.append(f"    {repeated_kw}{field.proto_type} {field.proto_name} = {tag};")
        tag += 1
    lines.append("}")
    return "\n".join(lines)


def generate_proto_text(partitions: Dict[str, List[ParsedField]], car_name:str) -> str:
    # Keep a fixed ordering for deterministic output
    partition_order = ["Dynamics", "Controls", "Pack", "DiagnosticsHigh", "DiagnosticsLow", "Thermal"]

    lines: List[str] = []
    lines.append('syntax = "proto3";')
    lines.append("")

    # Top-level message
    lines.append(f"message {car_name}SensorData {{")
    lines.append("    int64 time = 1;")
    lines.append("    int64 packet_id = 2;")

    # Match template.proto tag conventions (stable external schema expectations)
    if partitions.get("Dynamics"):
        lines.append("    Dynamics dynamics = 3;")
    if partitions.get("Controls"):
        lines.append("    Controls controls = 4;")
    if partitions.get("Pack"):
        lines.append("    Pack pack = 5;")
    if partitions.get("DiagnosticsHigh"):
        lines.append("    DiagnosticsHigh diagnostics_high = 6;")
    if partitions.get("DiagnosticsLow"):
        lines.append("    DiagnosticsLow diagnostics_low = 7;")
    if partitions.get("Thermal"):
        lines.append("    Thermal thermal = 8;")
    lines.append("}")
    lines.append("")

    # Partition messages
    for part in partition_order:
        fields = partitions.get(part, [])
        if not fields:
            continue
        lines.append(_emit_message(part, fields))
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a proto3 schema from CAN packets CSV.")
    parser.add_argument("can_csv", help="Path to can_packets.csv")
    parser.add_argument("bitfield_csv", help="Path to can_bitfields.csv")
    parser.add_argument("--out", required=True, help="Output .proto file")
    args = parser.parse_args()

    gen_json = _load_generate_can_json_module()
    bitfield_defs = gen_json.load_bitfield_definitions(args.bitfield_csv)
    packets = gen_json.process_csv(args.can_csv, bitfield_defs, args.bitfield_csv)

    partitions = parse_can_model_to_partitions(packets)
    proto_text = generate_proto_text(partitions, "Orion")
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(proto_text)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
