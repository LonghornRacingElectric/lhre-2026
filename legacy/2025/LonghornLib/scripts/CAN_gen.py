import csv
import json
import re
import math
import os

# --- Configuration ---
CAN_CSV_FILENAME = "NCAN_packets.csv"
BITFIELD_CSV_FILENAME = "NCAN_bitfields.csv"
JSON_FILENAME = "can_packets.json"

# --- Regex Definitions ---
# Regex for CAN signal: (type, context/precision)
# Group 1: Type (e.g., uint16, bitfield)
# Group 2: Context (e.g., 0.1, BSE Faults)
can_signal_pattern = re.compile(r"\(([^,]+)(?:,\s*([^)\s]+))?.*\)", re.IGNORECASE)

# Regex for power-of-2 precision
pow2_pattern = re.compile(r"2\^(-?\d+)")

# Regex for Protobuf: field_name[index](type) or field_name(type)
# Group 1: Field name
# Group 2: Optional index (digits)
# Group 3: Type
protobuf_pattern = re.compile(r"([\w\.]+)(?:\[(\d+)\])?\s*\(([^)]+)\)")

# --- Type Definitions ---
type_lengths = {
    "uint8": 1,
    "int8": 1,
    "uint16": 2,
    "int16": 2,
    "uint32": 4,
    "int32": 4,
    "uint64": 8,
    "int64": 8,
    "bool": 1,
    "boolean": 1,
    "byte": 1,
    "float": 4,
    "float32": 4,
    "double": 8,
    "float64": 8,
    "bitfield": 1,  # Default size for bitfield byte, adjust if needed
}
type_normalization = {"boolean": "uint8", "bool": "uint8", "byte": "uint8"}

# --- Helper Functions ---


def parse_dlc(dlc_str):
    dlc_str = dlc_str.strip()
    if not dlc_str:
        return 0
    range_match = re.match(r"\(\s*(\d+)\s*-\s*(\d+)\s*\)", dlc_str)
    if range_match:
        return int(range_match.group(2))
    try:
        return int(dlc_str)
    except ValueError:
        if dlc_str.lower() == "depends":
            return 0
        print(f"Warning: Could not parse DLC '{dlc_str}'. Defaulting to 0.")
        return 0


def parse_frequency(freq_str):
    freq_str = freq_str.strip().lower()
    if not freq_str or freq_str == "na" or freq_str == "0":
        return None
    try:
        return float(freq_str)
    except ValueError:
        print(f"Warning: Could not parse Frequency '{freq_str}'.")
        return None


def calculate_frequency_ms(frequency_hz):
    if frequency_hz is not None and frequency_hz > 0:
        try:
            return int(round((1.0 / frequency_hz) * 1000.0, 4))
        except ZeroDivisionError:
            return None
    return None


def parse_quantity(quantity_str):
    quantity_str = quantity_str.strip()
    if not quantity_str or quantity_str.lower() in ["depends", "max 32"]:
        return 0
    try:
        return int(quantity_str)
    except ValueError:
        print(f"Warning: Could not parse Quantity '{quantity_str}'. Defaulting to 0.")
        return 0


def parse_participants(participant_str):
    participant_str = participant_str.strip()
    if not participant_str:
        return []
    return [participant_str]


def load_bitfield_definitions(filepath):
    """Loads bitfield definitions from the specified CSV file."""
    definitions = {}
    if not os.path.exists(filepath):
        print(
            f"Warning: Bitfield definition file not found at '{filepath}'. Bitfield lookups will fail."
        )
        return definitions
    try:
        with open(filepath, mode="r", encoding="utf-8-sig") as csvfile:
            filtered_lines = filter(lambda row: row.strip(), csvfile)
            reader = csv.DictReader(filtered_lines)
            if reader.fieldnames is None:
                print(f"Error: Could not read headers from {filepath}.")
                return definitions
            bit_col_names = {}
            found_cols = 0
            for i in range(8):
                patterns_to_try = [f"b[{i}]", f"b[{i}] (lsb)", f"b[{i}] "]
                found = False
                for col_name_in_header in reader.fieldnames:
                    if any(
                        col_name_in_header.startswith(pattern)
                        for pattern in patterns_to_try
                    ):
                        bit_col_names[i] = col_name_in_header
                        found = True
                        found_cols += 1
                        break
                if not found and found_cols > 0:
                    print(
                        f"Warning: Could not find CSV header for bit index {i} in {filepath}"
                    )
                bit_col_names[i] = bit_col_names.get(i)  # Assign None if not found

            if "Bitfield" not in reader.fieldnames:
                print(f"Error: Required header 'Bitfield' not found in {filepath}.")
                return {}
            for i, row in enumerate(reader):
                row_num = i + 2
                bitfield_name = row.get("Bitfield", "").strip()
                if not bitfield_name:
                    continue
                bits_list = []
                for bit_index in range(8):
                    col_name = bit_col_names.get(bit_index)
                    if not col_name:
                        continue
                    cell_content = row.get(col_name, "").strip()
                    if ";" in cell_content:
                        try:
                            protobuf_field = cell_content.split(";", 1)[1].strip()
                            if protobuf_field:
                                bits_list.append(
                                    {
                                        "protobuf_field": protobuf_field,
                                        "bit_index": bit_index,
                                    }
                                )
                        except IndexError:
                            print(
                                f"Warning: Row {row_num} in {filepath}, Bit {bit_index}: Malformed content '{cell_content}'."
                            )
                if bits_list:
                    definitions[bitfield_name] = bits_list
    except Exception as e:
        print(f"Error loading bitfield definitions from {filepath}: {e}")
    print(f"Loaded {len(definitions)} bitfield definitions.")
    return definitions


def process_csv(can_filepath, bitfield_definitions):
    """Reads the main CAN CSV file and converts it to JSON, using bitfield definitions."""
    can_packets = []
    if not os.path.exists(can_filepath):
        print(f"Error: CAN definition file not found at '{can_filepath}'")
        return []
    try:
        with open(can_filepath, mode="r", encoding="utf-8-sig") as csvfile:
            reader = csv.DictReader(filter(lambda row: row.strip(), csvfile))
            processed_rows = 0
            for i, row in enumerate(reader):
                row_num = i + 2
                try:
                    # --- 1. Get Packet ID ---
                    packet_id_str = row.get("CAN ID", "").strip()
                    packet_id = None
                    if packet_id_str:
                        try:
                            packet_id = int(packet_id_str, 16)
                        except ValueError:
                            print(
                                f"Warning: Row {row_num}: Invalid hex CAN ID '{packet_id_str}'. Skip."
                            )
                            continue
                    else:
                        continue  # Skip rows without CAN ID

                    # --- 2. Get Other Basic Fields ---
                    packet_name = (
                        row.get("Packet Info", "").strip() or f"Packet_{packet_id_str}"
                    )
                    from_participants = parse_participants(row.get("From", ""))
                    to_participants = parse_participants(row.get("To", ""))
                    dlc = parse_dlc(row.get("Data Length Code (DLC)", "0"))
                    frequency_hz = parse_frequency(row.get("Frequency (Hz)", "NA"))
                    frequency_ms = calculate_frequency_ms(frequency_hz)
                    quantity = parse_quantity(row.get("Quantity", "0"))

                    # --- 3. Parse Data Bytes ---
                    bytes_list = []
                    current_byte_index = 0
                    field_index_counter = 0
                    for byte_num in range(8):
                        data_key = f"Data[{byte_num}]"
                        byte_info_str_raw = row.get(data_key, "").strip()
                        byte_info_str = byte_info_str_raw.strip('"')
                        if (
                            not byte_info_str
                            or byte_info_str.lower() == "unused"
                            or byte_info_str == ","
                        ):
                            continue

                        # --- Split CAN and Protobuf parts (Protobuf part is optional) ---
                        can_part = byte_info_str
                        protobuf_info = None
                        if ";" in byte_info_str:
                            parts = byte_info_str.split(";", 1)
                            can_part = parts[0].strip()
                            proto_part_raw = parts[1].strip()
                            proto_match = protobuf_pattern.search(proto_part_raw)
                            if proto_match:
                                proto_field, proto_index_str, proto_type = (
                                    proto_match.groups()
                                )
                                proto_field = proto_field.strip()
                                proto_type = proto_type.strip()
                                protobuf_info = {
                                    "type": proto_type,
                                    "field": proto_field,
                                }
                                if proto_index_str:
                                    protobuf_info["repeated"] = True
                                    try:
                                        protobuf_info["field_index"] = int(
                                            proto_index_str
                                        )
                                    except ValueError:
                                        print(
                                            f"Warning: Row {row_num}, ID {packet_id_str}, Data[{byte_num}]: Invalid protobuf index '{proto_index_str}'."
                                        )
                                        protobuf_info["field_index"] = None
                                else:
                                    protobuf_info["repeated"] = False
                                    protobuf_info["field_index"] = None
                            else:
                                print(
                                    f"Warning: Row {row_num}, ID {packet_id_str}, Data[{byte_num}]: Could not parse protobuf format in '{proto_part_raw}'."
                                )

                        # --- Parse the CAN signal part ---
                        bitfield_encoding_details = None  # Reset for each field
                        can_match = can_signal_pattern.search(can_part)
                        if can_match:
                            match_start_index = can_match.start()
                            name = (
                                can_part[:match_start_index].strip()
                                or f"Field_{field_index_counter}"
                            )  # This is the byte/signal name
                            type_str = (
                                can_match.group(1).lower().strip()
                                if can_match.group(1)
                                else ""
                            )
                            context_str = (
                                can_match.group(2).strip()
                                if can_match.group(2)
                                else None
                            )  # Precision OR Bitfield Name

                            normalized_type = type_normalization.get(type_str, type_str)
                            length = type_lengths.get(normalized_type)
                            if length is None:
                                print(
                                    f"Warning: Row {row_num}, ID {packet_id_str}: Unknown CAN type '{type_str}'. Assuming length 1 for field '{name}'."
                                )
                                length = 1
                                conv_type = type_str
                            else:
                                conv_type = normalized_type

                            precision = 1.0  # Default

                            # --- Check if it's a bitfield type ---
                            # <<< MODIFICATION START >>>
                            if conv_type == "bitfield":
                                if (
                                    context_str
                                ):  # context_str should hold the bitfield type name
                                    bitfield_lookup_key = context_str
                                    encoding_details = bitfield_definitions.get(
                                        bitfield_lookup_key
                                    )
                                    if encoding_details:
                                        bitfield_encoding_details = encoding_details
                                    else:
                                        print(
                                            f"Warning: Row {row_num}, ID {packet_id_str}: Bitfield type '{bitfield_lookup_key}' referenced for field '{name}' but no definition found in {BITFIELD_CSV_FILENAME}."
                                        )
                                else:
                                    # Missing the actual bitfield name after the comma
                                    print(
                                        f"Error: Row {row_num}, ID {packet_id_str}: Field '{name}' declared as (bitfield) but missing the specific bitfield type name after the comma."
                                    )
                                precision = (
                                    1.0  # Bitfields usually don't have float precision
                                )
                            # <<< MODIFICATION END >>>
                            elif (
                                context_str
                            ):  # If not bitfield, parse context as precision
                                precision_str = context_str  # Already stripped
                                pow2_match = pow2_pattern.match(precision_str)
                                if pow2_match:
                                    try:
                                        exponent = int(pow2_match.group(1))
                                        precision = math.pow(2, exponent)
                                    except ValueError:
                                        print(
                                            f"Warning: Row {row_num}, ID {packet_id_str}: Invalid exponent '{precision_str}'. Using 1.0 for field '{name}'."
                                        )
                                else:
                                    try:
                                        numeric_part = re.match(
                                            r"([-+]?\d*\.?\d+([eE][-+]?\d+)?)",
                                            precision_str,
                                        )
                                        if numeric_part:
                                            precision = float(numeric_part.group(1))
                                        elif precision_str.isalpha():
                                            precision = 1.0
                                        else:
                                            print(
                                                f"Warning: Row {row_num}, ID {packet_id_str}: Could not parse CAN precision '{precision_str}'. Using 1.0 for field '{name}'."
                                            )
                                    except ValueError:
                                        print(
                                            f"Warning: Row {row_num}, ID {packet_id_str}: Invalid CAN precision format '{precision_str}'. Using 1.0 for field '{name}'."
                                        )

                            # --- Create Byte Definition ---
                            if byte_num > current_byte_index:
                                current_byte_index = byte_num
                            byte_def = {
                                "index": field_index_counter,
                                "start_byte": current_byte_index,
                                "name": name,  # This remains the signal name
                                "length": length,
                                "conv_type": conv_type,
                                "precision": precision,
                            }
                            if protobuf_info:
                                byte_def["protobuf"] = protobuf_info
                            if bitfield_encoding_details:
                                byte_def["bitfield_encoding"] = (
                                    bitfield_encoding_details
                                )
                            bytes_list.append(byte_def)
                            field_index_counter += 1
                            current_byte_index += length
                        else:
                            # Handle simple CAN type definitions like "VSM (byte)"
                            simple_match = re.match(r"\(\s*(\w+)\s*\)", can_part)
                            if simple_match:
                                type_str = simple_match.group(1).lower().strip()
                                normalized_type = type_normalization.get(
                                    type_str, type_str
                                )
                                length = type_lengths.get(normalized_type)
                                if length:
                                    paren_idx = can_part.find("(")
                                    name = (
                                        can_part[:paren_idx].strip()
                                        or f"Field_{field_index_counter}"
                                    )
                                    if byte_num > current_byte_index:
                                        current_byte_index = byte_num
                                    byte_def = {
                                        "index": field_index_counter,
                                        "start_byte": current_byte_index,
                                        "name": name,
                                        "length": length,
                                        "conv_type": normalized_type,
                                        "precision": 1.0,
                                    }
                                    if protobuf_info:
                                        byte_def["protobuf"] = protobuf_info
                                    bytes_list.append(byte_def)
                                    field_index_counter += 1
                                    current_byte_index += length
                                else:
                                    print(
                                        f"Warning: Row {row_num}, ID {packet_id_str}: Simple CAN type '{type_str}' unknown length in '{can_part}'."
                                    )
                            # else: print(f"Warning: Row {row_num}, ID {packet_id_str}: Could not parse CAN signal in '{can_part}'.")

                    # --- 4. Construct Packet JSON ---
                    if packet_id is not None:
                        can_packet_json = {
                            "packet_id": packet_id,
                            "packet_name": packet_name,
                            "from": from_participants,
                            "to": to_participants,
                            "data_length": dlc,
                            "frequency_ms": frequency_ms,
                            "frequency": frequency_hz,
                            "quantity": quantity,
                            "bytes": bytes_list,
                        }
                        can_packets.append(can_packet_json)
                        processed_rows += 1

                except ValueError as ve:
                    print(
                        f"Error processing row {row_num} (ValueError): {ve}. Skipping row. Data: {row}"
                    )
                except KeyError as ke:
                    print(
                        f"Error processing row {row_num} (Missing Key): {ke}. Check CSV headers. Skipping row. Data: {row}"
                    )
                except Exception as e:
                    print(
                        f"Unexpected error processing row {row_num}: {e}. Skipping row. Data: {row}"
                    )

    except FileNotFoundError:
        print(f"Error: CAN definition file not found at '{can_filepath}'")
        return []
    except Exception as e:
        print(f"An critical error occurred while reading the CAN CSV file: {e}")
        return []

    print(f"Total CAN rows processed successfully: {processed_rows}")
    return can_packets


# --- Main Execution ---
if __name__ == "__main__":
    print(f"Loading bitfield definitions from: {BITFIELD_CSV_FILENAME}")
    bitfield_defs = load_bitfield_definitions(BITFIELD_CSV_FILENAME)

    print(f"Processing CAN definitions from: {CAN_CSV_FILENAME}")
    processed_packets = process_csv(CAN_CSV_FILENAME, bitfield_defs)

    if processed_packets:
        json_output = json.dumps(processed_packets, indent=4)
        try:
            with open(JSON_FILENAME, "w", encoding="utf-8") as outfile:
                outfile.write(json_output)
            print(f"\nOutput successfully written to {JSON_FILENAME}")
        except IOError as e:
            print(f"\nError writing JSON to file: {e}")
    else:
        print("No valid CAN packets were processed or an error occurred.")
