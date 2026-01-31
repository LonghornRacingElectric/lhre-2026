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
    """Infer proto type from the *same model* used to generate can_ids.{h,c}.

    Aligns with C generator semantics:
    - float/double stay float
    - scaled integer values (precision != 1.0) become float
    - bitfields remain an integer container for now
    - bool intent is preserved via byte_info["is_boolean"] when available
    """

    if byte_info.get("is_boolean") is True:
        return "bool"

    conv_type = (byte_info.get("conv_type") or "").lower()
    precision = byte_info.get("precision", 1.0)
    try:
        precision_f = float(precision)
    except (TypeError, ValueError):
        precision_f = 1.0

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
    if any(x in from_field_l for x in ["undertray", "upright"]):
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

    # Diagnostics: faults, errors, shutdowns, bitfields, etc.
    if any(k in name for k in [
        "fault",
        "error",
        "shutdown",
        "disconnect",
        "out_range",
        "mismatch",
        "implause",
        "imd",
        "bmb",
        "fuse",
    ]):
        # Split by frequency: higher rate -> DiagnosticsHigh.
        if frequency_hz is not None and frequency_hz >= 50.0:
            return "DiagnosticsHigh"
        return "DiagnosticsLow"

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


def _field_name_from_byte_info(byte_info: dict) -> Tuple[str, bool]:
    """Returns (proto_field_name, repeated)."""
    proto_meta = byte_info.get("protobuf")
    if isinstance(proto_meta, dict) and proto_meta.get("field"):
        field_name = str(proto_meta["field"]).strip()
        # keep only last segment of dotted names
        if "." in field_name:
            field_name = field_name.split(".")[-1]
        repeated = bool(proto_meta.get("repeated"))
        return (_to_snake_case(field_name), repeated)

    # Fallback to the same style as C generation for derived names
    name = str(byte_info.get("name") or "field").strip()
    return (_to_snake_case(name), False)


def parse_can_model_to_partitions(packets: list) -> Dict[str, List[ParsedField]]:
    partitions: Dict[str, Dict[str, ParsedField]] = {}
    repeated_bases: Dict[str, Set[str]] = {}

    for packet in packets:
        packet_info = str(packet.get("packet_name") or "").strip()
        from_list = packet.get("from") or []
        to_list = packet.get("to") or []
        frequency_hz = packet.get("frequency")
        try:
            frequency_hz_f: Optional[float] = float(frequency_hz) if frequency_hz is not None else None
        except (TypeError, ValueError):
            frequency_hz_f = None

        if not any(str(x).strip().lower() == "pi" for x in to_list):
            continue

        # Match prior packet-level logic: join lists to strings
        from_field = ", ".join(str(x) for x in from_list)

        for byte_info in packet.get("bytes") or []:
            proto_name, repeated = _field_name_from_byte_info(byte_info)
            proto_type = _infer_proto_type_from_can_model(byte_info)

            partition = _partition_for_field(from_field, packet_info, proto_name, frequency_hz_f)
            partitions.setdefault(partition, {})
            repeated_bases.setdefault(partition, set())

            if repeated:
                repeated_bases[partition].add(proto_name)

            parsed = ParsedField(proto_name=proto_name, proto_type=proto_type, repeated=repeated)

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
                )

    out: Dict[str, List[ParsedField]] = {}
    for partition, fields_map in partitions.items():
        fields: List[ParsedField] = []
        for name, fdef in fields_map.items():
            if name in repeated_bases.get(partition, set()):
                fdef = ParsedField(proto_name=fdef.proto_name, proto_type=fdef.proto_type, repeated=True)
            fields.append(fdef)
        out[partition] = sorted(fields, key=lambda f: f.proto_name)

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


def generate_proto_text(partitions: Dict[str, List[ParsedField]]) -> str:
    # Keep a fixed ordering for deterministic output
    partition_order = ["Dynamics", "Controls", "Pack", "DiagnosticsHigh", "DiagnosticsLow", "Thermal"]

    lines: List[str] = []
    lines.append('syntax = "proto3";')
    lines.append("")

    # Top-level message
    lines.append("message SensorData {")
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
        lines.append("    DiagnosticsLow diagnostics_low = 1001;")
    if partitions.get("Thermal"):
        lines.append("    Thermal thermal = 1002;")
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
    proto_text = generate_proto_text(partitions)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(proto_text)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
