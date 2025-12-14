import json
import argparse
import re
import os
import math
from functools import reduce

# --- Type Mappings ---
# Maps JSON conv_type to C type for RAW storage (wire format)
C_TYPE_MAP = {
    "float": "float",
    "uint8": "uint8_t",
    "int8": "int8_t",
    "uint16": "uint16_t",
    "int16": "int16_t",
    "uint32": "uint32_t",
    "int32": "int32_t",
    "double": "double",
}

# Maps bitfield 'length' (in bytes) to its C container type
BITFIELD_LEN_TO_TYPE = {
    1: "uint8_t",
    2: "uint16_t",
    4: "uint32_t",
    8: "uint64_t",
}


def to_macro_name(name):
    """Converts a readable name to an uppercase C macro name."""
    s1 = re.sub(r"[^\w]+", "_", name)
    s1 = s1.strip("_")
    return s1.upper()


def to_snake_case_name(name):
    """Converts a readable name to a lowercase C snake_case name."""
    return to_macro_name(name).lower()


def get_precision(byte_info):
    """Helper to safely get precision as a float, defaulting to 1.0."""
    try:
        return float(byte_info.get("precision", 1.0))
    except (ValueError, TypeError):
        return 1.0


def get_raw_c_type(byte_info):
    """Gets the C type for the raw wire format."""
    conv_type = byte_info["conv_type"]
    length = byte_info["length"]

    if conv_type == "bitfield":
        return BITFIELD_LEN_TO_TYPE.get(length)
    else:
        return C_TYPE_MAP.get(conv_type)


def get_app_type(byte_info):
    """
    Gets the C type used in the application struct.
    If precision != 1.0 and it's an integer type, use float.
    """
    raw_type = get_raw_c_type(byte_info)
    precision = get_precision(byte_info)
    conv_type = byte_info["conv_type"]

    # If the raw type is already floating point, keep it.
    if conv_type in ["float", "double"]:
        return raw_type

    # If precision implies scaling on an integer type, the app sees a float.
    if precision != 1.0 and raw_type is not None:
        return "float"

    return raw_type


def calculate_freq_gcd(json_data):
    """Extracts all frequencies and calculates the Greatest Common Divisor."""
    frequencies = []
    for packet in json_data:
        freq_ms_value = packet.get("frequency_ms")
        if freq_ms_value is not None:
            try:
                val = int(float(freq_ms_value))
                if val > 0:
                    frequencies.append(val)
            except (ValueError, TypeError):
                pass

    if not frequencies:
        return 1  # Default to 1 if no frequencies found to avoid divide by zero issues

    return reduce(math.gcd, frequencies)


def generate_header_content(json_data, input_filename, output_header_name):
    """Generates the C header file (.h) content."""

    header_lines = []
    header_guard = os.path.basename(output_header_name).upper().replace(".", "_")

    header_lines.append(f"#ifndef {header_guard}")
    header_lines.append(f"#define {header_guard}")
    header_lines.append("")
    header_lines.append("// Auto-generated CAN packet definition header file")
    header_lines.append(f"// Generated from: {input_filename}")
    header_lines.append("// DO NOT EDIT MANUALLY")
    header_lines.append("")
    header_lines.append("#include <stdint.h>")
    header_lines.append("#include <string.h>")
    header_lines.append("")

    # --- 1. GCD Calculation ---
    gcd_freq = calculate_freq_gcd(json_data)
    header_lines.append(f"// GCD of all packet frequencies")
    header_lines.append(f"#define CAN_FREQ_GCD {gcd_freq}")
    header_lines.append("")

    # --- 2. Bitfield Macros ---
    header_lines.append("// Generic Bitfield Manipulation Macros")
    header_lines.append("// Extracts 'width' bits starting at 'start_bit' from 'value'")
    header_lines.append(f"#define CAN_EXTRACT_BITFIELD(value, start_bit, width) \\")
    header_lines.append(f"    (((value) >> (start_bit)) & ((1ULL << (width)) - 1))")
    header_lines.append("")
    header_lines.append(
        "// Inserts 'field_val' into 'target' at 'start_bit' with 'width'"
    )
    header_lines.append(
        f"#define CAN_INSERT_BITFIELD(target, field_val, start_bit, width) do {{ \\"
    )
    header_lines.append(
        f"    (target) &= ~(((1ULL << (width)) - 1) << (start_bit)); \\"
    )
    header_lines.append(
        f"    (target) |= (((field_val) & ((1ULL << (width)) - 1)) << (start_bit)); \\"
    )
    header_lines.append(f"}} while(0)")
    header_lines.append("")

    packet_index = 0
    for packet in json_data:
        packet_index += 1
        try:
            packet_name = packet["packet_name"]
            packet_id = packet["packet_id"]
            data_length = packet["data_length"]
            bytes_data = packet.get("bytes", [])
            from_nodes = packet.get("from", [])
            to_nodes = packet.get("to", [])

            freq_ms_value = packet.get("frequency_ms")
            frequency_ms_int = 0
            if freq_ms_value is not None:
                try:
                    frequency_ms_int = int(float(freq_ms_value))
                except (ValueError, TypeError):
                    pass

            packet_macro_base = to_macro_name(packet_name)
            packet_snake_case = to_snake_case_name(packet_name)
            packet_struct_name = f"msg_{packet_snake_case}_t"

            header_lines.append(
                f"// =========================================================================="
            )
            header_lines.append(f"// Packet: {packet_name} ({packet_id})")
            header_lines.append(
                f"// =========================================================================="
            )
            if from_nodes:
                header_lines.append(f"// From: {', '.join(str(n) for n in from_nodes)}")
            if to_nodes:
                header_lines.append(f"// To:   {', '.join(str(n) for n in to_nodes)}")

            header_lines.append(f"#define {packet_macro_base}_ID {packet_id}")
            header_lines.append(f"#define {packet_macro_base}_DLC {data_length}")
            header_lines.append(f"#define {packet_macro_base}_FREQ {frequency_ms_int}")
            header_lines.append("")

            # --- Struct Definition ---
            header_lines.append(f"typedef struct {{")
            for byte_info in bytes_data:
                signal_name = byte_info.get("name", f"unk_signal_{packet_index}")
                member_name = to_snake_case_name(signal_name)

                # Use the application type (float if scaled)
                app_type = get_app_type(byte_info)

                if app_type:
                    header_lines.append(f"    {app_type} {member_name};")
                else:
                    header_lines.append(
                        f"    // Could not determine type for signal: {signal_name}"
                    )
            header_lines.append(f"}} {packet_struct_name};")
            header_lines.append("")

            # --- Signal Metadata Defines ---
            for byte_info in bytes_data:
                try:
                    signal_name = byte_info["name"]
                    conv_type = byte_info["conv_type"]
                    signal_macro_name = to_macro_name(signal_name)
                    signal_macro_base = f"{packet_macro_base}_{signal_macro_name}"

                    if conv_type == "bitfield":
                        bitfield_encoding = byte_info.get("bitfield_encoding", [])
                        header_lines.append(f"// Bitfield Indices for: {signal_name}")
                        for bit_spec in bitfield_encoding:
                            protobuf_field = bit_spec["protobuf_field"]
                            bit_index = bit_spec["bit_index"]
                            p_macro = to_macro_name(protobuf_field)
                            header_lines.append(
                                f"#define {signal_macro_base}_{p_macro}_IDX {bit_index}"
                            )

                    elif conv_type in C_TYPE_MAP:
                        precision = get_precision(byte_info)
                        header_lines.append(f"// Signal: {signal_name}")
                        header_lines.append(
                            f"#define {signal_macro_base}_PREC {precision}f"
                        )

                    header_lines.append("")

                except Exception as e:
                    print(f"Error processing metadata for '{signal_name}': {e}")

            header_lines.append(
                f"int pack_{packet_snake_case}(const {packet_struct_name}* msg, uint8_t* tx_buf);"
            )
            header_lines.append(
                f"int unpack_{packet_snake_case}(const uint8_t* rx_buf, {packet_struct_name}* msg);"
            )
            header_lines.append("")

        except Exception as e:
            print(f"Error processing packet header: {e}")

    header_lines.append(f"#endif // {header_guard}")
    header_lines.append("")

    return "\n".join(header_lines)


def generate_source_content(json_data, output_header_name):
    """Generates the C source file (.c) content."""

    source_lines = []
    header_basename = os.path.basename(output_header_name)

    source_lines.append("// Auto-generated CAN packet implementation file")
    source_lines.append(f"// Implements functions declared in: {header_basename}")
    source_lines.append("// DO NOT EDIT MANUALLY")
    source_lines.append("")
    source_lines.append(f'#include "longhorn/can/{header_basename}"')
    source_lines.append("")

    for packet in json_data:
        try:
            packet_name = packet["packet_name"]
            bytes_data = packet.get("bytes", [])
            packet_macro_base = to_macro_name(packet_name)
            packet_snake_case = to_snake_case_name(packet_name)
            packet_struct_name = f"msg_{packet_snake_case}_t"

            source_lines.append(f"// Packet: {packet_name}")

            # --- Pack Function ---
            source_lines.append(
                f"int pack_{packet_snake_case}(const {packet_struct_name}* msg, uint8_t* tx_buf) {{"
            )
            source_lines.append(f"    memset(tx_buf, 0, {packet_macro_base}_DLC);")
            source_lines.append("")

            for byte_info in bytes_data:
                signal_name = byte_info["name"]
                member_name = to_snake_case_name(signal_name)
                start_byte = byte_info["start_byte"]
                length = byte_info["length"]
                conv_type = byte_info["conv_type"]
                precision = get_precision(byte_info)

                raw_type = get_raw_c_type(byte_info)
                app_type = get_app_type(byte_info)

                if not raw_type:
                    continue

                source_lines.append(f"    // Pack: {signal_name}")

                # Determine the variable name holding the raw integer value to be packed
                val_to_pack = f"msg->{member_name}"

                # If application uses float but wire uses int, we must scale
                if app_type == "float" and raw_type != "float" and raw_type != "double":
                    # Create a temporary raw variable
                    val_to_pack = f"raw_{member_name}"
                    source_lines.append(
                        f"    {raw_type} {val_to_pack} = ({raw_type})(msg->{member_name} / {precision}f);"
                    )

                # Generate packing logic using 'val_to_pack'
                if conv_type == "float" and length == 4:
                    source_lines.append(
                        f"    memcpy(&tx_buf[{start_byte}], &{val_to_pack}, 4);"
                    )
                elif conv_type == "double" and length == 8:
                    source_lines.append(
                        f"    memcpy(&tx_buf[{start_byte}], &{val_to_pack}, 8);"
                    )
                elif length == 1:
                    source_lines.append(
                        f"    tx_buf[{start_byte}] = (uint8_t){val_to_pack};"
                    )
                elif length == 2:
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 0] = (uint8_t)({val_to_pack} & 0xFF);"
                    )
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 1] = (uint8_t)(({val_to_pack} >> 8) & 0xFF);"
                    )
                elif length == 4:
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 0] = (uint8_t)({val_to_pack} & 0xFF);"
                    )
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 1] = (uint8_t)(({val_to_pack} >> 8) & 0xFF);"
                    )
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 2] = (uint8_t)(({val_to_pack} >> 16) & 0xFF);"
                    )
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 3] = (uint8_t)(({val_to_pack} >> 24) & 0xFF);"
                    )
                elif length == 8:
                    for i in range(8):
                        source_lines.append(
                            f"    tx_buf[{start_byte} + {i}] = (uint8_t)(({val_to_pack} >> {i*8}) & 0xFF);"
                        )

                source_lines.append("")

            source_lines.append("    return 0;")
            source_lines.append("}")
            source_lines.append("")

            # --- Unpack Function ---
            source_lines.append(
                f"int unpack_{packet_snake_case}(const uint8_t* rx_buf, {packet_struct_name}* msg) {{"
            )

            for byte_info in bytes_data:
                signal_name = byte_info["name"]
                member_name = to_snake_case_name(signal_name)
                start_byte = byte_info["start_byte"]
                length = byte_info["length"]
                conv_type = byte_info["conv_type"]
                precision = get_precision(byte_info)

                raw_type = get_raw_c_type(byte_info)
                app_type = get_app_type(byte_info)

                if not raw_type:
                    continue

                source_lines.append(f"    // Unpack: {signal_name}")

                # 1. Extract RAW value from buffer
                # If scaling is involved, we read into a temp var first
                dest_var = f"msg->{member_name}"
                needs_scaling = (
                    app_type == "float" and raw_type != "float" and raw_type != "double"
                )

                if needs_scaling:
                    dest_var = f"raw_{member_name}"
                    source_lines.append(f"    {raw_type} {dest_var} = 0;")

                if conv_type == "float" and length == 4:
                    source_lines.append(
                        f"    memcpy(&{dest_var}, &rx_buf[{start_byte}], 4);"
                    )
                elif conv_type == "double" and length == 8:
                    source_lines.append(
                        f"    memcpy(&{dest_var}, &rx_buf[{start_byte}], 8);"
                    )
                elif length == 1:
                    # Cast to raw_type to preserve sign for int8
                    source_lines.append(
                        f"    {dest_var} = ({raw_type})rx_buf[{start_byte}];"
                    )
                elif length == 2:
                    source_lines.append(
                        f"    {dest_var} = ({raw_type})rx_buf[{start_byte} + 0];"
                    )
                    source_lines.append(
                        f"    {dest_var} |= ({raw_type})(rx_buf[{start_byte} + 1] << 8);"
                    )
                elif length == 4:
                    source_lines.append(
                        f"    {dest_var} = ({raw_type})rx_buf[{start_byte} + 0];"
                    )
                    source_lines.append(
                        f"    {dest_var} |= ({raw_type})(rx_buf[{start_byte} + 1] << 8);"
                    )
                    source_lines.append(
                        f"    {dest_var} |= ({raw_type})(rx_buf[{start_byte} + 2] << 16);"
                    )
                    source_lines.append(
                        f"    {dest_var} |= ({raw_type})(rx_buf[{start_byte} + 3] << 24);"
                    )
                elif length == 8:
                    source_lines.append(f"    {dest_var} = 0;")
                    for i in range(8):
                        source_lines.append(
                            f"    {dest_var} |= ({raw_type})((uint64_t)rx_buf[{start_byte} + {i}] << {i*8});"
                        )

                # 2. Apply scaling if necessary
                if needs_scaling:
                    source_lines.append(
                        f"    msg->{member_name} = (float){dest_var} * {precision}f;"
                    )

                source_lines.append("")

            source_lines.append("    return 0;")
            source_lines.append("}")
            source_lines.append("")

        except Exception as e:
            print(f"Error processing packet source: {e}")

    return "\n".join(source_lines)


def write_file(content, filename):
    """Writes content to a file, handling errors."""
    try:
        with open(filename, "w") as f:
            f.write(content)
        return True
    except IOError as e:
        print(f"Error writing to output file '{filename}': {e}")
        return False


# --- Main Execution ---
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate C header and source files for CAN IDs from a JSON file."
    )
    parser.add_argument("input", help="Path to the input JSON file")
    parser.add_argument("--h-file", required=True, help="Output C header file")
    parser.add_argument("--c-file", required=True, help="Output C source file")

    args = parser.parse_args()

    output_header_file = args.h_file
    output_source_file = args.c_file
    input_file = args.input

    try:
        h_dir = os.path.dirname(output_header_file)
        if h_dir:
            os.makedirs(h_dir, exist_ok=True)
        c_dir = os.path.dirname(output_source_file)
        if c_dir:
            os.makedirs(c_dir, exist_ok=True)
    except OSError as e:
        print(f"Error: Could not create output directory: {e}")
        exit(1)

    try:
        with open(input_file, "r") as f:
            can_data = json.load(f)
    except Exception as e:
        print(f"Error reading input file: {e}")
        exit(1)

    if not isinstance(can_data, list):
        print("Error: Input JSON must be a list.")
        exit(1)

    print("Generating header file content...")
    header_content = generate_header_content(can_data, input_file, output_header_file)

    print("Generating source file content...")
    source_content = generate_source_content(can_data, output_header_file)

    if write_file(header_content, output_header_file):
        print(f"Successfully generated '{output_header_file}'")

    if write_file(source_content, output_source_file):
        print(f"Successfully generated '{output_source_file}'")
