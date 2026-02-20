import json
import argparse
import re
import os

# --- Configuration ---
DEFAULT_INPUT_FILENAME = "can_packets.json"
DEFAULT_OUTPUT_FILENAME = "night_can_ids.h"


def to_macro_name(name):
    """Converts a readable name to an uppercase C macro name."""
    # Replace non-alphanumeric characters (except _) with underscore
    s1 = re.sub(r"[^\w]+", "_", name)
    # Remove leading/trailing underscores that might result
    s1 = s1.strip("_")
    # Convert to uppercase
    return s1.upper()


def generate_header(json_data, input_filename, output_filename=DEFAULT_OUTPUT_FILENAME):
    """Generates the C header file content from the parsed JSON data."""

    header_lines = []
    header_guard = os.path.basename(output_filename).upper().replace(".", "_")

    # --- Header Start ---
    header_lines.append(f"#ifndef {header_guard}")
    header_lines.append(f"#define {header_guard}")
    header_lines.append("")
    header_lines.append("// Auto-generated CAN ID header file")
    header_lines.append(f"// Generated from: {input_filename}")
    header_lines.append("// DO NOT EDIT MANUALLY")
    header_lines.append("")
    header_lines.append("#include <stdint.h> // For fixed-width integer types")
    header_lines.append("")

    # --- Type Mapping ---
    c_type_map = {
        "float": "float",
        "uint8": "uint8_t",
        "int8": "int8_t",
        "uint16": "uint16_t",
        "int16": "int16_t",
        "uint32": "uint32_t",
        "int32": "int32_t",
        "double": "double",
        # Bitfield needs _BYTE and _LENGTH, but not _TYPE or _PREC at the signal level
        "bitfield": "/* N/A (bitfield) */",
    }

    # --- Process Each Packet ---
    packet_index = 0  # For better error reporting if packet_name is missing
    for packet in json_data:
        packet_index += 1
        packet_name_for_error = (
            f"at index {packet_index - 1}"  # Default error identifier
        )
        try:
            # --- Mandatory Fields ---
            packet_name = packet["packet_name"]
            packet_name_for_error = f"'{packet_name}'"  # Use name if available
            packet_id = packet["packet_id"]
            data_length = packet["data_length"]

            # --- Optional Fields with Defaults/Handling ---
            quantity = packet.get("quantity", 1)  # Default quantity if missing
            bytes_data = packet.get("bytes", [])  # Default to empty list
            from_nodes = packet.get("from", [])  # Get 'from' list, default to empty
            to_nodes = packet.get("to", [])  # Get 'to' list, default to empty

            # --- Handle frequency_ms (allowing for null/None) ---
            freq_ms_value = packet.get("frequency_ms")
            frequency_ms_int = 0  # Default frequency value
            if freq_ms_value is not None:
                try:
                    frequency_ms_int = int(float(freq_ms_value))
                except (ValueError, TypeError):
                    print(
                        f"Warning: Invalid or non-numeric value '{freq_ms_value}' for 'frequency_ms' in packet {packet_name_for_error}. Using default 0."
                    )
            else:
                print(
                    f"Warning: 'frequency_ms' is null or missing for packet {packet_name_for_error}. Using default frequency 0."
                )

            # --- Start Generating Header Lines for Packet ---
            packet_macro_base = to_macro_name(packet_name)
            header_lines.append(f"// Packet: {packet_name}")
            if from_nodes:
                header_lines.append(f"// From: {', '.join(str(n) for n in from_nodes)}")
            if to_nodes:
                header_lines.append(f"// To:   {', '.join(str(n) for n in to_nodes)}")

            # --- Add Packet Defines ---
            header_lines.append(f"#define {packet_macro_base}_ID {packet_id}")
            header_lines.append(f"#define {packet_macro_base}_DLC {data_length}")
            header_lines.append(f"#define {packet_macro_base}_FREQ {frequency_ms_int}")
            header_lines.append(f"#define {packet_macro_base}_QUANTITY {quantity}")
            header_lines.append("")

            # --- Process Each Byte/Signal in the Packet ---
            signal_index = 0
            for byte_info in bytes_data:
                signal_index += 1
                signal_name_for_error = (
                    f"signal index {signal_index - 1} in packet {packet_name_for_error}"
                )
                try:
                    # --- Extract Common Signal Info ---
                    signal_name = byte_info["name"]
                    signal_name_for_error = (
                        f"'{signal_name}' in packet {packet_name_for_error}"
                    )
                    start_byte = byte_info["start_byte"]
                    length = byte_info["length"]
                    conv_type = byte_info["conv_type"]

                    signal_macro_name = to_macro_name(signal_name)
                    signal_macro_base = f"{packet_macro_base}_{signal_macro_name}"

                    # --- Generate Common Signal Defines (_BYTE, _LENGTH) ---
                    # These apply to both standard signals and bitfield containers
                    header_lines.append(
                        f"#define {signal_macro_base}_BYTE {start_byte}"
                    )
                    header_lines.append(f"#define {signal_macro_base}_LENGTH {length}")

                    # --- Handle Type-Specific Defines ---
                    if conv_type == "bitfield":
                        # Bitfields don't have signal-level _PREC or _TYPE
                        # Generate the individual bit index defines instead
                        bitfield_encoding = byte_info.get("bitfield_encoding")
                        if not bitfield_encoding or not isinstance(
                            bitfield_encoding, list
                        ):
                            print(
                                f"Warning: Missing or invalid 'bitfield_encoding' list for signal {signal_name_for_error}. Skipping bitfield index defines."
                            )
                            header_lines.append(
                                f"// Warning: Bitfield index definitions skipped for {signal_name} due to missing/invalid encoding data."
                            )
                        else:
                            header_lines.append(
                                f"// --- Bitfield Indices for: {signal_name} ---"
                            )
                            bit_index_count = 0
                            for bit_spec in bitfield_encoding:
                                try:
                                    protobuf_field = bit_spec["protobuf_field"]
                                    bit_index = bit_spec["bit_index"]

                                    max_bit_index = (length * 8) - 1
                                    if (
                                        not isinstance(bit_index, int)
                                        or bit_index < 0
                                        or bit_index > max_bit_index
                                    ):
                                        print(
                                            f"Warning: Invalid 'bit_index' ({bit_index}) for protobuf_field '{protobuf_field}' in signal {signal_name_for_error}. Expected 0-{max_bit_index}. Skipping this bit."
                                        )
                                        continue

                                    protobuf_macro_name = to_macro_name(protobuf_field)
                                    bit_macro_name = (
                                        f"{signal_macro_base}_{protobuf_macro_name}_IDX"
                                    )
                                    header_lines.append(
                                        f"#define {bit_macro_name} {bit_index}"
                                    )
                                    bit_index_count += 1

                                except KeyError as e:
                                    print(
                                        f"Warning: Missing key {e} in bitfield specification within signal {signal_name_for_error}. Skipping this bit."
                                    )
                                except Exception as e:
                                    print(
                                        f"Error processing bit definition in signal {signal_name_for_error}: {e}. Skipping this bit."
                                    )

                            if (
                                bit_index_count == 0
                            ):  # Remove the "--- Bitfield Indices ---" comment if no bits were defined
                                header_lines.pop()  # Remove the last added comment line

                    else:
                        # --- Generate Defines for Standard Numeric/Float Types (_PREC, _TYPE) ---
                        precision = byte_info["precision"]
                        c_type = c_type_map.get(conv_type)  # Get type from map

                        if c_type is None:
                            print(
                                f"Warning: Unknown conversion type '{conv_type}' for signal {signal_name_for_error}. Omitting _TYPE define."
                            )
                            header_lines.append(
                                f"// Warning: Unknown conversion type '{conv_type}' - _TYPE define omitted."
                            )
                        else:
                            header_lines.append(
                                f"#define {signal_macro_base}_TYPE {c_type}"
                            )

                        try:
                            precision_float = float(precision)
                            header_lines.append(
                                f"#define {signal_macro_base}_PREC {precision_float}f"
                            )
                        except (ValueError, TypeError):
                            print(
                                f"Warning: Invalid precision value '{precision}' for signal {signal_name_for_error}. Omitting _PREC define."
                            )
                            header_lines.append(
                                f"// Warning: Invalid precision value '{precision}' - _PREC define omitted."
                            )

                    # Add a blank line after each signal definition block (standard or bitfield)
                    header_lines.append("")

                except KeyError as e:
                    print(
                        f"Warning: Missing required key {e} in signal definition {signal_name_for_error}. Skipping signal generation."
                    )
                    # Add a comment indicating the skip
                    header_lines.append(
                        f"// Warning: Signal definition skipped for {signal_name_for_error} due to missing key: {e}"
                    )
                    header_lines.append("")
                except Exception as e:
                    print(
                        f"Error processing signal {signal_name_for_error}: {e}. Skipping signal generation."
                    )
                    header_lines.append(
                        f"// Error: Signal definition skipped for {signal_name_for_error} due to error: {e}"
                    )
                    header_lines.append("")

            header_lines.append("// End Packet: " + packet_name)
            header_lines.append("")  # Blank line after packet definition

        except KeyError as e:
            print(
                f"Warning: Missing required key {e} in top-level packet definition {packet_name_for_error}. Skipping packet."
            )
        except Exception as e:  # Catch other potential errors during packet processing
            print(
                f"Error processing packet {packet_name_for_error}: {e}. Skipping packet."
            )

    # --- Header End ---
    header_lines.append(f"#endif // {header_guard}")
    header_lines.append("")  # Ensure trailing newline

    # --- Write to File ---
    try:
        with open(output_filename, "w") as f:
            f.write("\n".join(header_lines))
        print(f"Successfully generated '{output_filename}' from '{input_filename}'")
    except IOError as e:
        print(f"Error writing to output file '{output_filename}': {e}")
        exit(1)


# --- Main Execution ---
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=f"Generate a C header file for CAN IDs from '{DEFAULT_INPUT_FILENAME}'."
    )
    parser.add_argument(
        "-o",
        "--output",
        default=DEFAULT_OUTPUT_FILENAME,
        help=f"Path to the output C header file (default: {DEFAULT_OUTPUT_FILENAME})",
    )

    args = parser.parse_args()
    output_file = args.output

    input_file = DEFAULT_INPUT_FILENAME
    try:
        with open(input_file, "r") as f:
            try:
                can_data = json.load(f)
            except json.JSONDecodeError as e:
                print(f"Error: Invalid JSON format in '{input_file}': {e}")
                exit(1)
    except FileNotFoundError:
        print(f"Error: Input JSON file not found: '{input_file}'")
        print(
            f"Please ensure '{DEFAULT_INPUT_FILENAME}' exists in the same directory as the script."
        )
        exit(1)
    except IOError as e:
        print(f"Error reading input file '{input_file}': {e}")
        exit(1)

    if not isinstance(can_data, list):
        print(
            f"Error: The top-level structure in '{input_file}' must be a JSON array (list)."
        )
        exit(1)

    generate_header(can_data, input_file, output_file)
