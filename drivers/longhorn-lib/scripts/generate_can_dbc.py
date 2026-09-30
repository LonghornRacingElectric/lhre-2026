#!/usr/bin/env python3
import csv
import re
import sys
import os
import math

# --- Type and Normalization Constants ---
TYPE_LENGTHS = {
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
    "bitfield": 1,
}

TYPE_NORMALIZATION = {
    "boolean": "uint8",
    "bool": "uint8",
    "byte": "uint8"
}

SIGNED_TYPES = {
    "int8",
    "int16",
    "int32",
    "int64",
    "float",
    "float32",
    "double",
    "float64"
}

# --- Regex Patterns ---
can_signal_pattern = re.compile(r"\(([^,]+)(?:,\s*([^)\s]+))?.*\)", re.IGNORECASE)
pow2_pattern = re.compile(r"2\^(-?\d+)")

# --- Helper Functions ---
def to_macro_name(name):
    """Converts a readable name to an uppercase C macro name."""
    s1 = re.sub(r"[^\w]+", "_", name)
    s1 = s1.strip("_")
    return s1.upper()

def to_snake_case_name(name):
    """Converts a readable name to a lowercase C snake_case name."""
    return to_macro_name(name).lower()

def clean_message_name(name):
    """Converts a packet name to a valid DBC message name."""
    s = re.sub(r"[^\w]+", "_", name)
    return s.strip("_")

def shorten_name(name, max_len=32):
    """Shortens a name to max_len characters using common automotive abbreviations."""
    if len(name) <= max_len:
        return name
    
    replacements = [
        ("suspension_potentiometer", "sus_pot"),
        ("suspension", "sus"),
        ("potentiometer", "pot"),
        ("front_left", "fl"),
        ("front_right", "fr"),
        ("back_left", "bl"),
        ("back_right", "br"),
        ("rear_left", "rl"),
        ("rear_right", "rr"),
        ("firmware_update", "fw_upd"),
        ("firmware", "fw"),
        ("update", "upd"),
        ("command", "cmd"),
        ("response", "resp"),
        ("packet", "pkt"),
        ("status", "sts"),
        ("temperature", "temp"),
        ("voltage", "volt"),
        ("current", "curr"),
        ("acceleration", "accel"),
        ("displacement", "disp"),
    ]
    
    short_name = name
    for old, new in replacements:
        # Match word boundaries or exact substrings
        short_name = re.sub(r"\b" + old + r"\b", new, short_name, flags=re.IGNORECASE)
        short_name = re.sub(old, new, short_name, flags=re.IGNORECASE)
        if len(short_name) <= max_len:
            return short_name
            
    return short_name[:max_len].strip("_")

def parse_participants(participant_str):
    participant_str = participant_str.strip()
    if not participant_str:
        return []
    return [p.strip() for p in participant_str.split(",") if p.strip()]

def load_bitfield_definitions(filepath):
    """Loads bitfield definitions from the specified CSV file."""
    definitions = {}
    if not os.path.exists(filepath):
        print(f"Warning: Bitfield definition file not found at '{filepath}'.")
        return definitions

    try:
        with open(filepath, mode="r", encoding="utf-8-sig") as csvfile:
            filtered_lines = filter(lambda row: row.strip(), csvfile)
            reader = csv.DictReader(filtered_lines)
            if reader.fieldnames is None:
                return definitions

            # Locate columns like b[0], b[1], etc.
            bit_col_names = {}
            for i in range(8):
                patterns_to_try = [f"b[{i}]", f"b[{i}] (lsb)", f"b[{i}] "]
                for col_name_in_header in reader.fieldnames:
                    if any(col_name_in_header.startswith(pattern) for pattern in patterns_to_try):
                        bit_col_names[i] = col_name_in_header
                        break

            if "Bitfield" not in reader.fieldnames:
                print(f"Error: Required header 'Bitfield' not found in {filepath}.")
                return {}

            for row in reader:
                bitfield_name = row.get("Bitfield", "").strip()
                if not bitfield_name:
                    continue
                
                bits_dict = {}
                for bit_index in range(8):
                    col_name = bit_col_names.get(bit_index)
                    if not col_name:
                        continue
                    cell_content = row.get(col_name, "").strip()
                    if ";" in cell_content:
                        parts = cell_content.split(";", 1)
                        description = parts[0].strip()
                        signal_name = parts[1].strip()
                        bits_dict[bit_index] = (description, to_snake_case_name(signal_name))
                
                if bits_dict:
                    definitions[bitfield_name] = bits_dict
    except Exception as e:
        print(f"Error loading bitfield definitions from {filepath}: {e}")
    return definitions

def parse_precision_and_unit(context_str):
    """Parses precision and unit from a context string (e.g., '0.1C', '2^-7 rad/s')."""
    if not context_str:
        return 1.0, ""
    
    context_str = context_str.strip()
    
    # Check for power-of-2 format
    pow2_match = pow2_pattern.match(context_str)
    if pow2_match:
        exponent = int(pow2_match.group(1))
        precision = math.pow(2, exponent)
        unit = context_str[pow2_match.end():].strip()
        return precision, unit

    # Match standard numeric prefix
    num_match = re.match(r"([-+]?\d*\.?\d+([eE][-+]?\d+)?)", context_str)
    if num_match:
        precision = float(num_match.group(1))
        unit = context_str[num_match.end():].strip()
        return precision, unit

    # Non-numeric context (like a bitfield name or 'bool' string)
    return 1.0, ""

def generate_dbc(can_csv_path, bitfield_csv_path, dbc_out_path):
    # Load bitfield definitions
    bitfield_defs = load_bitfield_definitions(bitfield_csv_path)

    # Collect nodes and packets
    nodes = set(["Vector__XXX"])
    packets = []

    with open(can_csv_path, mode="r", encoding="utf-8-sig") as csvfile:
        reader = csv.DictReader(filter(lambda row: row.strip(), csvfile))
        for row in reader:
            packet_id_str = row.get("CAN ID", "").strip()
            if not packet_id_str:
                continue
            
            try:
                packet_id = int(packet_id_str, 16)
            except ValueError:
                continue
            
            packet_name = row.get("Packet Info", "").strip() or f"Packet_{packet_id_str}"
            from_nodes = parse_participants(row.get("From", ""))
            to_nodes = parse_participants(row.get("To", ""))
            
            # Add to node declarations
            for node in from_nodes + to_nodes:
                if node != "*":
                    nodes.add(node)
            
            # Parse DLC
            dlc_str = row.get("Data Length Code (DLC)", "0").strip()
            dlc = 8
            if dlc_str:
                try:
                    dlc = int(dlc_str)
                except ValueError:
                    pass

            # Parse Quantity
            qty_str = row.get("Quantity", "1").strip()
            quantity = 1
            if qty_str:
                try:
                    quantity = max(1, int(qty_str))
                except ValueError:
                    pass

            # Parse signals in Data[0] to Data[7]
            signals = []
            current_byte_index = 0
            
            for byte_num in range(8):
                data_key = f"Data[{byte_num}]"
                byte_info_str = row.get(data_key, "").strip()
                if not byte_info_str or byte_info_str.lower() == "unused" or byte_info_str == ",":
                    continue
                
                # Split CAN and Protobuf parts
                can_part = byte_info_str
                if ";" in byte_info_str:
                    can_part = byte_info_str.split(";", 1)[0].strip()

                can_match = can_signal_pattern.search(can_part)
                if can_match:
                    name = can_part[:can_match.start()].strip()
                    type_str = can_match.group(1).lower().strip()
                    context_str = can_match.group(2).strip() if can_match.group(2) else None
                    
                    normalized_type = TYPE_NORMALIZATION.get(type_str, type_str)
                    length_bytes = TYPE_LENGTHS.get(normalized_type, 1)
                    
                    if byte_num > current_byte_index:
                        current_byte_index = byte_num

                    if normalized_type == "bitfield" and context_str:
                        # This is a bitfield signal
                        signals.append({
                            "type": "bitfield",
                            "name": name,
                            "bitfield_name": context_str,
                            "start_byte": current_byte_index,
                            "length_bytes": length_bytes,
                        })
                    else:
                        # Normal signal
                        precision, unit = parse_precision_and_unit(context_str)
                        is_signed = normalized_type in SIGNED_TYPES
                        
                        signals.append({
                            "type": "normal",
                            "name": name,
                            "start_byte": current_byte_index,
                            "length_bytes": length_bytes,
                            "is_signed": is_signed,
                            "precision": precision,
                            "unit": unit,
                        })
                    
                    current_byte_index += length_bytes
                else:
                    # Simple type like "(byte)" or "(bool)" without complex details
                    simple_match = re.search(r"\(\s*(\w+)\s*\)", can_part)
                    if simple_match:
                        type_str = simple_match.group(1).lower().strip()
                        normalized_type = TYPE_NORMALIZATION.get(type_str, type_str)
                        length_bytes = TYPE_LENGTHS.get(normalized_type, 1)
                        name = can_part[:can_part.find("(")].strip()
                        
                        if byte_num > current_byte_index:
                            current_byte_index = byte_num

                        signals.append({
                            "type": "normal",
                            "name": name,
                            "start_byte": current_byte_index,
                            "length_bytes": length_bytes,
                            "is_signed": normalized_type in SIGNED_TYPES,
                            "precision": 1.0,
                            "unit": "",
                        })
                        current_byte_index += length_bytes

            packets.append({
                "id": packet_id,
                "name": packet_name,
                "from": from_nodes,
                "to": to_nodes,
                "dlc": dlc,
                "quantity": quantity,
                "signals": signals,
            })

    # Generate DBC File Content
    lines = []
    lines.append("VERSION \"\"")
    lines.append("")
    lines.append("")
    lines.append("NS_ : ")
    lines.append("    NS_DESC_")
    lines.append("    CM_")
    lines.append("    BA_DEF_")
    lines.append("    BA_")
    lines.append("    VAL_")
    lines.append("    VAL_TABLE_")
    lines.append("    BA_DEF_DEF_")
    lines.append("    BA_DEF_SGTYPE_")
    lines.append("    BA_SGTYPE_")
    lines.append("    BA_DEF_BO_SGTYPE_")
    lines.append("    BA_BO_SGTYPE_")
    lines.append("    SGTYPE_")
    lines.append("    SGTYPE_VAL_")
    lines.append("    FILTER")
    lines.append("    BU_BO_REL_")
    lines.append("    BU_SG_REL_")
    lines.append("    BU_EV_REL_")
    lines.append("    DEFINE_BO_GR_")
    lines.append("    VECTOR_EQU_")
    lines.append("    SGTYPE_VAL_")
    lines.append("")
    lines.append("BS_:")
    lines.append("")
    
    # Emit Nodes
    nodes_sorted = sorted(list(nodes))
    lines.append(f"BU_: {' '.join(nodes_sorted)}")
    lines.append("")
    lines.append("")

    comments = [] # List of tuples: (type, id, item_name, comment_text)

    # Process Packets and Signals
    for packet in packets:
        # Determine number of signals with '[i]' to do proper indexing
        i_signals = [s for s in packet["signals"] if re.search(r"\[i(?:\+\d+)?\]", s["name"])]
        num_i_signals = len(i_signals)
        
        # Overlapping ID resolution:
        # Only treat quantity as sequential if it has at least one indexed signal [i]
        is_sequential = (packet["quantity"] > 1) and (num_i_signals > 0)
        actual_quantity = packet["quantity"] if is_sequential else 1

        # Loop over quantity (instances)
        for p in range(actual_quantity):
            msg_id = packet["id"] + p
            msg_name_base = clean_message_name(packet["name"])
            
            # Format message name: append index if quantity > 1
            raw_msg_name = f"{msg_name_base}_{p}" if is_sequential else msg_name_base
            msg_name = shorten_name(raw_msg_name, 32)
            
            sender = packet["from"][0] if packet["from"] else "Vector__XXX"
            
            # DLC Auto-correction: Ensure DLC is at least big enough to fit all signals
            max_byte_needed = 0
            for signal in packet["signals"]:
                max_byte_needed = max(max_byte_needed, signal["start_byte"] + signal["length_bytes"])
            
            msg_dlc = max(packet["dlc"], max_byte_needed)
            
            lines.append(f"BO_ {msg_id} {msg_name}: {msg_dlc} {sender}")
            
            # Collect message comment
            comments.append(("BO_", msg_id, None, packet["name"]))
            
            # Process signals in this packet instance
            for signal in packet["signals"]:
                receivers = " ".join(packet["to"]) if packet["to"] and packet["to"] != ["*"] else "Vector__XXX"
                
                # Helper function to compute signal name and description
                def get_formatted_name_and_desc(raw_name):
                    match = re.search(r"\[i(?:\+(\d+))?\]", raw_name)
                    if match:
                        offset = int(match.group(1)) if match.group(1) else 0
                        absolute_idx = p * num_i_signals + offset
                        base_name = re.sub(r"\[i(?:\+\d+)?\]", "", raw_name).strip()
                        sig_name = shorten_name(to_snake_case_name(f"{base_name} {absolute_idx}"), 32)
                        sig_desc = f"{base_name} {absolute_idx}"
                        return sig_name, sig_desc
                    else:
                        sig_name = shorten_name(to_snake_case_name(raw_name), 32)
                        sig_desc = raw_name
                        if is_sequential:
                            sig_name = shorten_name(f"{sig_name}_{p}", 32)
                            sig_desc = f"{sig_desc} {p}"
                        return sig_name, sig_desc

                if signal["type"] == "bitfield":
                    # Retrieve the bitfield definition
                    bf_name = signal["bitfield_name"]
                    bf_def = bitfield_defs.get(bf_name, {})
                    
                    if bf_def:
                        # For each bit in the bitfield definition, create a 1-bit boolean signal
                        for bit_idx, (bit_desc, bit_sig_name) in bf_def.items():
                            start_bit = signal["start_byte"] * 8 + bit_idx
                            
                            # Format name with instance suffix if needed
                            if is_sequential:
                                final_sig_name = shorten_name(f"{bit_sig_name}_{p}", 32)
                                final_sig_desc = f"{bit_desc} {p}"
                            else:
                                final_sig_name = shorten_name(bit_sig_name, 32)
                                final_sig_desc = bit_desc
                                
                            lines.append(f" SG_ {final_sig_name} : {start_bit}|1@1+ (1,0) [0|1] \"\" {receivers}")
                            comments.append(("SG_", msg_id, final_sig_name, final_sig_desc))
                    else:
                        # Unnamed/unresolved bitfield container
                        sig_name, sig_desc = get_formatted_name_and_desc(signal["name"])
                        start_bit = signal["start_byte"] * 8
                        bit_len = signal["length_bytes"] * 8
                        lines.append(f" SG_ {sig_name} : {start_bit}|{bit_len}@1+ (1,0) [0|{2**bit_len - 1}] \"\" {receivers}")
                        comments.append(("SG_", msg_id, sig_name, sig_desc))

                else:
                    # Normal signal
                    sig_name, sig_desc = get_formatted_name_and_desc(signal["name"])
                    
                    start_bit = signal["start_byte"] * 8
                    bit_len = signal["length_bytes"] * 8
                    sign_char = "-" if signal["is_signed"] else "+"
                    
                    # Calculate Min and Max ranges based on integer/float type
                    precision = signal["precision"]
                    if signal["is_signed"]:
                        min_val = -math.pow(2, bit_len - 1) * precision
                        max_val = (math.pow(2, bit_len - 1) - 1) * precision
                    else:
                        min_val = 0.0
                        max_val = (math.pow(2, bit_len) - 1) * precision

                    # If it's a float/double on the wire, don't define limits in the brackets
                    if "float" in signal.get("name", "").lower() or signal["precision"] == 1.0 and bit_len in [32, 64]:
                        min_val, max_val = 0.0, 0.0

                    unit = signal["unit"]
                    
                    lines.append(f" SG_ {sig_name} : {start_bit}|{bit_len}@1{sign_char} ({precision},0) [{min_val}|{max_val}] \"{unit}\" {receivers}")
                    comments.append(("SG_", msg_id, sig_name, sig_desc))
            
            lines.append("") # blank line between messages

    # Emit Comments (CM_)
    for comment_type, msg_id, sig_name, comment_text in comments:
        # Escape double quotes
        comment_text_esc = comment_text.replace('"', '\\"')
        if comment_type == "BO_":
            lines.append(f"CM_ BO_ {msg_id} \"{comment_text_esc}\";")
        elif comment_type == "SG_":
            lines.append(f"CM_ SG_ {msg_id} {sig_name} \"{comment_text_esc}\";")

    # Output DBC
    with open(dbc_out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines).strip() + "\n")
    print(f"Successfully generated DBC: {dbc_out_path}")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: generate_can_dbc.py <can_packets.csv> <can_bitfields.csv> <output.dbc>")
        sys.exit(1)
        
    generate_dbc(sys.argv[1], sys.argv[2], sys.argv[3])
