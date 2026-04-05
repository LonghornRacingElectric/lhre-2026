import json
from google.protobuf.descriptor import FieldDescriptor

from stack.ingest.protobuf import angelique_pb2, can_packets_pb2


def _as_list(value):
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        return list(value)
    return [value]


def _coerce_scalar(value, field):
    if field.type == FieldDescriptor.TYPE_BOOL:
        if isinstance(value, str):
            return value.strip().lower() in {"1", "true", "yes", "on"}
        return bool(value)

    if field.type in {
        FieldDescriptor.TYPE_INT32,
        FieldDescriptor.TYPE_INT64,
        FieldDescriptor.TYPE_SINT32,
        FieldDescriptor.TYPE_SINT64,
        FieldDescriptor.TYPE_SFIXED32,
        FieldDescriptor.TYPE_SFIXED64,
        FieldDescriptor.TYPE_UINT32,
        FieldDescriptor.TYPE_UINT64,
        FieldDescriptor.TYPE_FIXED32,
        FieldDescriptor.TYPE_FIXED64,
    }:
        return int(value)

    if field.type in {FieldDescriptor.TYPE_FLOAT, FieldDescriptor.TYPE_DOUBLE}:
        return float(value)

    if field.type == FieldDescriptor.TYPE_BYTES:
        if isinstance(value, bytes):
            return value
        if isinstance(value, bytearray):
            return bytes(value)
        if isinstance(value, str):
            return value.encode("utf-8")
        return b""

    if field.type == FieldDescriptor.TYPE_STRING:
        if isinstance(value, (dict, list)):
            return json.dumps(value)
        return str(value)

    return value


def _assign_message(message, values):
    if not isinstance(values, dict):
        return

    for field in message.DESCRIPTOR.fields:
        name = field.name
        if name not in values:
            continue

        value = values[name]
        if value is None:
            continue

        if field.label == FieldDescriptor.LABEL_REPEATED:
            if field.type == FieldDescriptor.TYPE_MESSAGE:
                repeated = getattr(message, name)
                for item in _as_list(value):
                    if not isinstance(item, dict):
                        continue
                    child = repeated.add()
                    _assign_message(child, item)
            else:
                repeated = getattr(message, name)
                for item in _as_list(value):
                    if item is None:
                        continue
                    repeated.append(_coerce_scalar(item, field))
            continue

        if field.type == FieldDescriptor.TYPE_MESSAGE:
            if isinstance(value, dict):
                _assign_message(getattr(message, name), value)
            continue

        setattr(message, name, _coerce_scalar(value, field))


def _build_payload(row_dict_list, idx, table_names):
    payload = {
        "time": int(row_dict_list.get("packet", [{}])[idx].get("time", 0) or 0),
        "packet_id": int(row_dict_list.get("packet", [{}])[idx].get("packet_id", 0) or 0),
    }

    for table in table_names:
        rows = row_dict_list.get(table, [])
        if idx < len(rows) and isinstance(rows[idx], dict):
            payload[table] = rows[idx]

    return payload


def build_angelique_sensor_data_proto(row_dict_list, idx):
    msg = angelique_pb2.AngeliqueSensorData()
    payload = _build_payload(row_dict_list, idx, ["dynamics", "controls", "pack", "diagnostics", "thermal"])
    _assign_message(msg, payload)
    return msg


def build_orion_sensor_data_proto(row_dict_list, idx):
    msg = can_packets_pb2.OrionSensorData()
    payload = _build_payload(
        row_dict_list,
        idx,
        ["dynamics", "controls", "pack", "diagnostics_high", "diagnostics_low", "thermal"],
    )
    _assign_message(msg, payload)
    return msg
