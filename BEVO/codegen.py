import json
import os
import sys
from google.protobuf import descriptor_pb2
from google.protobuf.descriptor_pool import DescriptorPool

def get_proto_mapping(desc_path):
    if not os.path.exists(desc_path):
        print(f"Error: Descriptor file {desc_path} not found.")
        sys.exit(1)

    with open(desc_path, 'rb') as f:
        fds = descriptor_pb2.FileDescriptorSet()
        fds.ParseFromString(f.read())
    
    pool = DescriptorPool()
    for file_desc in fds.file:
        pool.Add(file_desc)
    
    proto_full_name = 'SensorData'
    
    try:
        msg_desc = pool.FindMessageTypeByName(proto_full_name)
    except KeyError:
        print(f"Error: Could not find '{proto_full_name}' in the descriptor.")
        sys.exit(1)

    var_map = {
        "dynamics": "_d", 
        "controls": "_c",
        "pack": "_p",
        "diagnostics_low": "_l",
        "diagnostics_high": "_h",
        "thermal": "_t"
    }

    field_to_var = {}
    for field in msg_desc.fields:
        if field.message_type and field.name in var_map:
            var_prefix = var_map[field.name]
            for sub_field in field.message_type.fields:
                field_to_var[sub_field.name] = var_prefix
    
    return field_to_var

def generate_rust():
    # Adjusted paths for centralized BEVO root
    json_path = "../drivers/proto/can_packets.json"
    desc_path = "sensor_data.desc"
    output_path = "generated_mapping.rs"

    proto_map = get_proto_mapping(desc_path)

    try:
        with open(json_path, 'r') as f:
            packets = json.load(f)
    except FileNotFoundError:
        print(f"Error: Could not find JSON at {json_path}")
        return

    # Header and variable bindings with underscores
    code = [
        "// THIS FILE IS AUTO-GENERATED. DO NOT EDIT.",
        "use crate::proto::SensorData;",
        "use crate::config::ProtobufMapping;",
        "",
        "pub fn update_proto_field_generated(data: &mut SensorData, name: &str, val: f32, config: &ProtobufMapping) {",
        "    let _d = data.dynamics.get_or_insert_with(Default::default);",
        "    let _c = data.controls.get_or_insert_with(Default::default);",
        "    let _p = data.pack.get_or_insert_with(Default::default);",
        "    let _l = data.diagnostics_low.get_or_insert_with(Default::default);",
        "    let _h = data.diagnostics_high.get_or_insert_with(Default::default);",
        "    let _t = data.thermal.get_or_insert_with(Default::default);",
        "",
        "    if config.repeated {",
        "        if let Some(i) = config.field_index {",
        "            match name {"
    ]

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
                # Capture type for scalar fields
                scalar_signals[fname] = pb.get("type", "float")

            # Handle Bitfields
            if sig.get("conv_type") == "bitfield":
                mappings = sig.get("bitfield_encoding") or sig.get("mappings") or sig.get("bits") or []
                for m in mappings:
                    b_fname = m.get("protobuf_field") or m.get("field") or m.get("name")
                    if b_fname and b_fname in proto_map:
                        bool_signals.add(b_fname)
                        
    # Generate Repeated Match Arms
    for f in sorted(rep_signals):
        code.append(f'                "{f}" => crate::set_vec_index(&mut {proto_map.get(f, "_d")}.{f}, i, val),')

    code.extend(["                _ => (),", "            }", "        }", "    } else {", "        match name {"])

    # Generate Scalar Match Arms with Type Casting
    for f, ftype in sorted(scalar_signals.items()):
        prefix = proto_map[f]
        if ftype == "bool":
            # Float to Bool conversion for Rust safety
            code.append(f'            "{f}" => {prefix}.{f} = val != 0.0,')
        else:
            code.append(f'            "{f}" => {prefix}.{f} = val,')

    # Close first function, start bool function
    code.extend(["            _ => (),", "        }", "    }", "}", "", 
                 "pub fn update_proto_bool_generated(data: &mut SensorData, name: &str, val: bool) {",
                 "    let _l = data.diagnostics_low.get_or_insert_with(Default::default);",
                 "    let _h = data.diagnostics_high.get_or_insert_with(Default::default);",
                 "    let _t = data.thermal.get_or_insert_with(Default::default);",
                 "    match name {"])

    # Generate Bitfield Match Arms
    for f in sorted(bool_signals):
        code.append(f'        "{f}" => {proto_map.get(f, "_h")}.{f} = val,')

    code.extend(["        _ => (),", "    }", "}"])

    with open(output_path, 'w') as f:
        f.write("\n".join(code))
    print(f"Generated {output_path}")

if __name__ == "__main__":
    generate_rust()