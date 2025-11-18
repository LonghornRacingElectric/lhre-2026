import json
import argparse
import re
import os

# --- Type Mappings ---
# Maps JSON conv_type to C type for struct members
C_TYPE_MAP = {
    "float": "float",
    "uint8": "uint8_t",
    "int8": "int8_t",
    "uint16": "uint16_t",
    "int16": "int16_t",
    "uint32": "uint32_t",
    "int32": "int32_t",
    "double": "double",
    # bitfield is handled separately by length
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


def get_c_type_for_signal(byte_info):
    """Gets the C type for a struct member based on its signal info."""
    conv_type = byte_info["conv_type"]
    length = byte_info["length"]

    if conv_type == "bitfield":
        c_type = BITFIELD_LEN_TO_TYPE.get(length)
        if c_type is None:
            print(
                f"Warning: Bitfield '{byte_info.get('name', 'N/A')}' has unsupported length {length}. Skipping."
            )
            return None
        return c_type
    else:
        c_type = C_TYPE_MAP.get(conv_type)
        if c_type is None:
            print(
                f"Warning: Signal '{byte_info.get('name', 'N/A')}' has unknown type '{conv_type}'. Skipping."
            )
            return None
        return c_type


def generate_header_content(json_data, input_filename, output_header_name):
    """Generates the C header file (.h) content."""

    header_lines = []
    # Use the basename of the provided .h file path for the header guard
    header_guard = os.path.basename(output_header_name).upper().replace(".", "_")

    # --- Header Start ---
    header_lines.append(f"#ifndef {header_guard}")
    header_lines.append(f"#define {header_guard}")
    header_lines.append("")
    header_lines.append("// Auto-generated CAN packet definition header file")
    header_lines.append(f"// Generated from: {input_filename}")
    header_lines.append("// DO NOT EDIT MANUALLY")
    header_lines.append("")
    header_lines.append("#include <stdint.h> // For fixed-width integer types")
    header_lines.append("#include <string.h> // For memcpy")
    header_lines.append("")

    packet_index = 0
    for packet in json_data:
        packet_index += 1
        packet_name_for_error = f"at index {packet_index - 1}"
        try:
            # --- Get Packet Info ---
            packet_name = packet["packet_name"]
            packet_name_for_error = f"'{packet_name}'"
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
                    pass  # Warning already printed in main script, just use 0

            # --- Base Names ---
            packet_macro_base = to_macro_name(packet_name)
            packet_snake_case = to_snake_case_name(packet_name)
            packet_struct_name = f"msg_{packet_snake_case}_t"

            # --- Start Generating Header Lines for Packet ---
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

            # --- Packet Defines ---
            header_lines.append(f"#define {packet_macro_base}_ID {packet_id}")
            header_lines.append(f"#define {packet_macro_base}_DLC {data_length}")
            header_lines.append(f"#define {packet_macro_base}_FREQ {frequency_ms_int}")
            header_lines.append("")

            # --- Struct Definition ---
            header_lines.append(f"typedef struct {{")
            for byte_info in bytes_data:
                signal_name = byte_info.get("name", f"unk_signal_{packet_index}")
                member_name = to_snake_case_name(signal_name)
                c_type = get_c_type_for_signal(byte_info)

                if c_type:
                    header_lines.append(f"    {c_type} {member_name};")
                else:
                    header_lines.append(
                        f"    // Could not determine type for signal: {signal_name}"
                    )
            header_lines.append(f"}} {packet_struct_name};")
            header_lines.append("")

            # --- Signal Metadata Defines (for application logic) ---
            for byte_info in bytes_data:
                try:
                    signal_name = byte_info["name"]
                    conv_type = byte_info["conv_type"]
                    signal_macro_name = to_macro_name(signal_name)
                    signal_macro_base = f"{packet_macro_base}_{signal_macro_name}"

                    if conv_type == "bitfield":
                        # Generate the individual bit index defines
                        bitfield_encoding = byte_info.get("bitfield_encoding", [])
                        header_lines.append(f"// Bitfield Indices for: {signal_name}")
                        for bit_spec in bitfield_encoding:
                            protobuf_field = bit_spec["protobuf_field"]
                            bit_index = bit_spec["bit_index"]
                            protobuf_macro_name = to_macro_name(protobuf_field)
                            bit_macro_name = (
                                f"{signal_macro_base}_{protobuf_macro_name}_IDX"
                            )
                            header_lines.append(f"#define {bit_macro_name} {bit_index}")

                    elif conv_type in C_TYPE_MAP:
                        # Generate _PREC define for numeric types
                        precision = byte_info["precision"]
                        precision_float = float(precision)
                        header_lines.append(f"// Signal: {signal_name}")
                        header_lines.append(
                            f"#define {signal_macro_base}_PREC {precision_float}f"
                        )

                    header_lines.append("")  # Blank line after each signal's metadata

                except KeyError as e:
                    print(
                        f"Warning: Missing key {e} in signal '{signal_name}' for packet '{packet_name}'. Metadata defines may be incomplete."
                    )
                except Exception as e:
                    print(f"Error processing signal metadata for '{signal_name}': {e}")

            # --- Function Declarations ---
            header_lines.append(f"/**")
            header_lines.append(f" * Pack {packet_name} message")
            header_lines.append(f" * @param msg     Pointer to struct to pack from")
            header_lines.append(
                f" * @param tx_buf  Pointer to 8-byte buffer to pack into"
            )
            header_lines.append(f" * @return 0 on success")
            header_lines.append(f" */")
            header_lines.append(
                f"int pack_{packet_snake_case}(const {packet_struct_name}* msg, uint8_t* tx_buf);"
            )
            header_lines.append("")

            header_lines.append(f"/**")
            header_lines.append(f" * Unpack {packet_name} message")
            header_lines.append(
                f" * @param rx_buf  Pointer to 8-byte buffer to unpack from"
            )
            header_lines.append(f" * @param msg     Pointer to struct to unpack into")
            header_lines.append(f" * @return 0 on success")
            header_lines.append(f" */")
            header_lines.append(
                f"int unpack_{packet_snake_case}(const uint8_t* rx_buf, {packet_struct_name}* msg);"
            )
            header_lines.append("")

        except KeyError as e:
            print(
                f"Warning: Missing required key {e} in top-level packet definition {packet_name_for_error}. Skipping packet."
            )
        except Exception as e:
            print(
                f"Error processing packet {packet_name_for_error}: {e}. Skipping packet."
            )

    # --- Header End ---
    header_lines.append(f"#endif // {header_guard}")
    header_lines.append("")

    return "\n".join(header_lines)


def generate_source_content(json_data, output_header_name):
    """Generates the C source file (.c) content."""

    source_lines = []
    # Get the basename of the .h file to #include it
    header_basename = os.path.basename(output_header_name)

    # --- Source Start ---
    source_lines.append("// Auto-generated CAN packet implementation file")
    source_lines.append(f"// Implements functions declared in: {header_basename}")
    source_lines.append("// DO NOT EDIT MANUALLY")
    source_lines.append("")
    source_lines.append(f'#include "{header_basename}"')
    source_lines.append("")

    packet_index = 0
    for packet in json_data:
        packet_index += 1
        try:
            # --- Get Packet Info ---
            packet_name = packet["packet_name"]
            bytes_data = packet.get("bytes", [])

            # --- Base Names ---
            packet_macro_base = to_macro_name(packet_name)
            packet_snake_case = to_snake_case_name(packet_name)
            packet_struct_name = f"msg_{packet_snake_case}_t"

            source_lines.append(
                f"// =========================================================================="
            )
            source_lines.append(f"// Packet: {packet_name}")
            source_lines.append(
                f"// =========================================================================="
            )

            # --- Pack Function Implementation ---
            pack_func_name = f"pack_{packet_snake_case}"
            source_lines.append(
                f"int {pack_func_name}(const {packet_struct_name}* msg, uint8_t* tx_buf) {{"
            )
            source_lines.append(f"    // Clear buffer")
            source_lines.append(f"    memset(tx_buf, 0, {packet_macro_base}_DLC);")
            source_lines.append("")

            for byte_info in bytes_data:
                signal_name = byte_info["name"]
                member_name = to_snake_case_name(signal_name)
                start_byte = byte_info["start_byte"]
                length = byte_info["length"]
                conv_type = byte_info["conv_type"]
                c_type = get_c_type_for_signal(byte_info)

                if not c_type:
                    source_lines.append(
                        f"    // Skipping pack for {signal_name} (unknown type)"
                    )
                    continue

                source_lines.append(
                    f"    // Pack: {signal_name} ({c_type}, {length} bytes)"
                )

                # Simple, byte-aligned, little-endian packing
                if conv_type == "float" and length == 4:
                    source_lines.append(
                        f"    memcpy(&tx_buf[{start_byte}], &msg->{member_name}, 4);"
                    )
                elif conv_type == "double" and length == 8:
                    source_lines.append(
                        f"    memcpy(&tx_buf[{start_byte}], &msg->{member_name}, 8);"
                    )
                elif (
                    conv_type == "uint8"
                    or conv_type == "int8"
                    or conv_type == "bitfield"
                ) and length == 1:
                    source_lines.append(
                        f"    tx_buf[{start_byte}] = (uint8_t)msg->{member_name};"
                    )
                elif (
                    conv_type == "uint16"
                    or conv_type == "int16"
                    or conv_type == "bitfield"
                ) and length == 2:
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 0] = (uint8_t)(msg->{member_name} & 0xFF);"
                    )
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 1] = (uint8_t)((msg->{member_name} >> 8) & 0xFF);"
                    )
                elif (
                    conv_type == "uint32"
                    or conv_type == "int32"
                    or conv_type == "bitfield"
                ) and length == 4:
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 0] = (uint8_t)(msg->{member_name} & 0xFF);"
                    )
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 1] = (uint8_t)((msg->{member_name} >> 8) & 0xFF);"
                    )
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 2] = (uint8_t)((msg->{member_name} >> 16) & 0xFF);"
                    )
                    source_lines.append(
                        f"    tx_buf[{start_byte} + 3] = (uint8_t)((msg->{member_name} >> 24) & 0xFF);"
                    )
                else:
                    source_lines.append(
                        f"    // PACK: Unhandled type/length combo for {signal_name}: {conv_type} / {length} bytes"
                    )

                source_lines.append("")

            source_lines.append("    return 0;")
            source_lines.append("}")
            source_lines.append("")

            # --- Unpack Function Implementation ---
            unpack_func_name = f"unpack_{packet_snake_case}"
            source_lines.append(
                f"int {unpack_func_name}(const uint8_t* rx_buf, {packet_struct_name}* msg) {{"
            )

            for byte_info in bytes_data:
                signal_name = byte_info["name"]
                member_name = to_snake_case_name(signal_name)
                start_byte = byte_info["start_byte"]
                length = byte_info["length"]
                conv_type = byte_info["conv_type"]
                c_type = get_c_type_for_signal(byte_info)

                if not c_type:
                    source_lines.append(
                        f"    // Skipping unpack for {signal_name} (unknown type)"
                    )
                    continue

                source_lines.append(
                    f"    // Unpack: {signal_name} ({c_type}, {length} bytes)"
                )

                # Simple, byte-aligned, little-endian unpacking
                if conv_type == "float" and length == 4:
                    source_lines.append(
                        f"    memcpy(&msg->{member_name}, &rx_buf[{start_byte}], 4);"
                    )
                elif conv_type == "double" and length == 8:
                    source_lines.append(
                        f"    memcpy(&msg->{member_name}, &rx_buf[{start_byte}], 8);"
                    )
                elif conv_type == "uint8" and length == 1:
                    source_lines.append(
                        f"    msg->{member_name} = rx_buf[{start_byte}];"
                    )
                elif conv_type == "int8" and length == 1:
                    source_lines.append(
                        f"    msg->{member_name} = (int8_t)rx_buf[{start_byte}];"
                    )
                elif (
                    conv_type == "bitfield" and length == 1
                ):  # Assumes bitfield is uint
                    source_lines.append(
                        f"    msg->{member_name} = rx_buf[{start_byte}];"
                    )
                elif conv_type == "uint16" and length == 2:
                    source_lines.append(
                        f"    msg->{member_name} = (uint16_t)(rx_buf[{start_byte} + 0]);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (uint16_t)(rx_buf[{start_byte} + 1] << 8);"
                    )
                elif conv_type == "int16" and length == 2:
                    source_lines.append(
                        f"    msg->{member_name} = (int16_t)(rx_buf[{start_byte} + 0]);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (int16_t)(rx_buf[{start_byte} + 1] << 8);"
                    )
                elif (
                    conv_type == "bitfield" and length == 2
                ):  # Assumes bitfield is uint
                    source_lines.append(
                        f"    msg->{member_name} = (uint16_t)(rx_buf[{start_byte} + 0]);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (uint16_t)(rx_buf[{start_byte} + 1] << 8);"
                    )
                elif conv_type == "uint32" and length == 4:
                    source_lines.append(
                        f"    msg->{member_name} = (uint32_t)(rx_buf[{start_byte} + 0]);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (uint32_t)(rx_buf[{start_byte} + 1] << 8);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (uint32_t)(rx_buf[{start_byte} + 2] << 16);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (uint32_t)(rx_buf[{start_byte} + 3] << 24);"
                    )
                elif conv_type == "int32" and length == 4:
                    source_lines.append(
                        f"    msg->{member_name} = (int32_t)(rx_buf[{start_byte} + 0]);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (int32_t)(rx_buf[{start_byte} + 1] << 8);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (int32_t)(rx_buf[{start_byte} + 2] << 16);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (int32_t)(rx_buf[{start_byte} + 3] << 24);"
                    )
                elif (
                    conv_type == "bitfield" and length == 4
                ):  # Assumes bitfield is uint
                    source_lines.append(
                        f"    msg->{member_name} = (uint32_t)(rx_buf[{start_byte} + 0]);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (uint32_t)(rx_buf[{start_byte} + 1] << 8);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (uint32_t)(rx_buf[{start_byte} + 2] << 16);"
                    )
                    source_lines.append(
                        f"    msg->{member_name} |= (uint32_t)(rx_buf[{start_byte} + 3] << 24);"
                    )
                else:
                    source_lines.append(
                        f"    // UNPACK: Unhandled type/length combo for {signal_name}: {conv_type} / {length} bytes"
                    )

                source_lines.append("")

            source_lines.append("    return 0;")
            source_lines.append("}")
            source_lines.append("")

        except KeyError as e:
            print(
                f"Warning: Missing required key {e} in packet at index {packet_index-1}. Skipping C function generation for this packet."
            )
        except Exception as e:
            print(
                f"Error processing packet at index {packet_index-1} for C file: {e}. Skipping C function generation."
            )

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
    parser.add_argument(
        "input",
        help="Path to the input JSON file (e.g., can_packets.json)",
    )
    parser.add_argument(
        "--h-file",
        required=True,
        help="Full path for the output C header file (e.g., my_can.h)",
    )
    parser.add_argument(
        "--c-file",
        required=True,
        help="Full path for the output C source file (e.g., my_can.c)",
    )
    # ---

    args = parser.parse_args()

    output_header_file = args.h_file
    output_source_file = args.c_file

    try:
        # Get directory part of the path
        h_dir = os.path.dirname(output_header_file)
        if h_dir:  # Only create if path is not in current dir (e.g., "src/my.h")
            os.makedirs(h_dir, exist_ok=True)

        c_dir = os.path.dirname(output_source_file)
        if c_dir:  # Only create if path is not in current dir
            os.makedirs(c_dir, exist_ok=True)
    except OSError as e:
        print(f"Error: Could not create output directory: {e}")
        exit(1)

    input_file = args.input

    # --- Read and Parse JSON ---
    try:
        with open(input_file, "r") as f:
            try:
                can_data = json.load(f)
            except json.JSONDecodeError as e:
                print(f"Error: Invalid JSON format in '{input_file}': {e}")
                exit(1)
    except FileNotFoundError:
        print(f"Error: Input JSON file not found: '{input_file}'")
        exit(1)
    except IOError as e:
        print(f"Error reading input file '{input_file}': {e}")
        exit(1)

    if not isinstance(can_data, list):
        print(
            f"Error: The top-level structure in '{input_file}' must be a JSON array (list)."
        )
        exit(1)

    # --- Generate Content ---
    print("Generating header file content...")
    # Pass the full header path to the generators
    header_content = generate_header_content(can_data, input_file, output_header_file)

    print("Generating source file content...")
    # The source generator needs the header path to know what to #include
    source_content = generate_source_content(can_data, output_header_file)

    # --- Write Files ---
    if write_file(header_content, output_header_file):
        print(f"Successfully generated '{output_header_file}'")

    if write_file(source_content, output_source_file):
        print(f"Successfully generated '{output_source_file}'")
