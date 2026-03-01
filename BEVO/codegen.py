import argparse
import json
import os
import sys
from google.protobuf import descriptor_pb2
from google.protobuf.descriptor_pool import DescriptorPool


def _iter_messages(file_desc):
    for msg in file_desc.message_type:
        full_name = f"{file_desc.package}.{msg.name}" if file_desc.package else msg.name
        yield full_name, msg


def resolve_sensor_message(file_desc_set, explicit_name=None):
    if explicit_name:
        for file_desc in file_desc_set.file:
            for full_name, msg_desc in _iter_messages(file_desc):
                if explicit_name in {full_name, msg_desc.name}:
                    return full_name, msg_desc
        print(f"Error: Could not find message '{explicit_name}' in descriptor.")
        sys.exit(1)

    for preferred in ["OrionSensorData", "SensorData"]:
        for file_desc in file_desc_set.file:
            for full_name, msg_desc in _iter_messages(file_desc):
                if preferred in {full_name, msg_desc.name}:
                    return full_name, msg_desc

    for file_desc in file_desc_set.file:
        for full_name, msg_desc in _iter_messages(file_desc):
            if msg_desc.name.endswith("SensorData"):
                return full_name, msg_desc

    print("Error: Could not find a '*SensorData' message in descriptor.")
    sys.exit(1)


def get_proto_mapping(desc_path, explicit_message=None):
    if not os.path.exists(desc_path):
        print(f"Error: Descriptor file {desc_path} not found.")
        sys.exit(1)

    with open(desc_path, "rb") as f:
        fds = descriptor_pb2.FileDescriptorSet()
        fds.ParseFromString(f.read())

    pool = DescriptorPool()
    for file_desc in fds.file:
        pool.Add(file_desc)

    message_name, msg_proto = resolve_sensor_message(fds, explicit_message)
    msg_desc = pool.FindMessageTypeByName(message_name)

    preferred_var_map = {
        "dynamics": "_d",
        "controls": "_c",
        "pack": "_p",
        "diagnostics_low": "_l",
        "diagnostics_high": "_h",
        "thermal": "_t",
    }

    field_to_var = {}
    field_bindings = []
    for field in msg_desc.fields:
        if field.message_type:
            var_prefix = preferred_var_map.get(field.name, f"_{field.name}")
            field_bindings.append((field.name, var_prefix))
            for sub_field in field.message_type.fields:
                field_to_var[sub_field.name] = var_prefix

    return field_to_var, field_bindings, message_name


def generate_rust(json_path, desc_path, output_path, message_name=None):
    proto_map, bindings, resolved_message = get_proto_mapping(desc_path, message_name)
    binding_lookup = {var_name: field_name for field_name, var_name in bindings}
    rust_message_path = resolved_message.replace(".", "::")
    rust_message_name = resolved_message.split(".")[-1]

    try:
        with open(json_path, "r") as f:
            packets = json.load(f)
    except FileNotFoundError:
        print(f"Error: Could not find JSON at {json_path}")
        return

    code = [
        "// THIS FILE IS AUTO-GENERATED. DO NOT EDIT.",
        f"use crate::proto::{rust_message_path};",
        "use crate::config::ProtobufMapping;",
        "",
        f"pub fn update_proto_field_generated(data: &mut {rust_message_name}, name: &str, val: f32, config: &ProtobufMapping) {{",
    ]
    for field_name, var_name in bindings:
        code.append(f"    let {var_name} = data.{field_name}.get_or_insert_with(Default::default);")
    code.extend([
        "",
        "    if config.repeated {",
        "        if let Some(i) = config.field_index {",
        "            match name {",
    ])

    scalar_signals = {}
    rep_signals = set()
    bool_signals = set()

    for pkt in packets:
        signals = pkt.get("bytes") or pkt.get("signals") or []
        for sig in signals:
            pb = sig.get("protobuf")
            if not pb or not isinstance(pb, dict):
                continue

            fname = pb.get("field_name") or pb.get("field") or pb.get("name")
            if not fname or fname not in proto_map:
                continue

            if pb.get("repeated", False):
                rep_signals.add(fname)
            else:
                scalar_signals[fname] = str(pb.get("type", "float")).lower()

            if sig.get("conv_type") == "bitfield":
                mappings = sig.get("bitfield_encoding") or sig.get("mappings") or sig.get("bits") or []
                for mapping in mappings:
                    bitfield_name = mapping.get("protobuf_field") or mapping.get("field") or mapping.get("name")
                    if bitfield_name and bitfield_name in proto_map:
                        bool_signals.add(bitfield_name)

    for field_name in sorted(rep_signals):
        code.append(
            f'                "{field_name}" => crate::set_vec_index(&mut {proto_map[field_name]}.{field_name}, i, val),'
        )

    code.extend([
        "                _ => (),",
        "            }",
        "        }",
        "    } else {",
        "        match name {",
    ])

    for field_name, field_type in sorted(scalar_signals.items()):
        prefix = proto_map[field_name]
        if field_type == "bool":
            code.append(f'            "{field_name}" => {prefix}.{field_name} = val != 0.0,')
        else:
            code.append(f'            "{field_name}" => {prefix}.{field_name} = val,')

    code.extend([
        "            _ => (),",
        "        }",
        "    }",
        "}",
        "",
        f"pub fn update_proto_bool_generated(data: &mut {rust_message_name}, name: &str, val: bool) {{",
    ])

    bool_prefixes = sorted({proto_map[field_name] for field_name in bool_signals})
    for prefix in bool_prefixes:
        field_name = binding_lookup[prefix]
        code.append(f"    let {prefix} = data.{field_name}.get_or_insert_with(Default::default);")

    code.append("    match name {")
    for field_name in sorted(bool_signals):
        code.append(f'        "{field_name}" => {proto_map[field_name]}.{field_name} = val,')

    code.extend(["        _ => (),", "    }", "}"])

    with open(output_path, "w") as f:
        f.write("\n".join(code))
    print(f"Generated {output_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate Rust protobuf field mapping from descriptor + CAN JSON")
    parser.add_argument("--json", dest="json_path", required=True)
    parser.add_argument("--desc", dest="desc_path", required=True)
    parser.add_argument("--out", dest="output_path", required=True)
    parser.add_argument("--message", dest="message_name", required=False)
    args = parser.parse_args()
    generate_rust(args.json_path, args.desc_path, args.output_path, args.message_name)