import re
import os
import sys


def snake_to_pascal(name):
    return "".join(part.capitalize() for part in name.split('_'))

# --- Type Mappings ---

PG_TO_PRISMA_TYPES = {
    'smallint': 'Int',
    'integer': 'Int',
    'bigint': 'BigInt',
    'smallserial': 'Int',
    'serial': 'Int',
    'bigserial': 'BigInt',
    'real': 'Float',
    'double precision': 'Float',
    'text': 'String',
    'varchar': 'String',
    'boolean': 'Boolean',
    'date': 'DateTime',
    'timestamp': 'DateTime',
    'timestamptz': 'DateTime',
    'jsonb': 'Json',
    'bytea': 'Bytes',
    'point': 'Unsupported("point")',
}

PROTO_TO_SQL_TYPES = {
    'float': 'real',
    'double': 'double precision',
    'int32': 'integer',
    'int64': 'bigint',
    'bool': 'boolean',
    'string': 'text',
    'bytes': 'bytea',
}

PROTO_TO_PRISMA_TYPES = {
    'float': 'Float',
    'double': 'Float',
    'int32': 'Int',
    'int64': 'BigInt',
    'bool': 'Boolean',
    'string': 'String',
    'bytes': 'Bytes',
}

PROTO_TO_SA_TYPES = {
    'float': 'Float',
    'double': 'Float',
    'int32': 'Integer',
    'int64': 'BigInteger',
    'bool': 'Boolean',
    'string': 'Text',
    'bytes': 'BYTEA',
}

PROTO_TO_PY_TYPES = {
    'float': 'float',
    'double': 'float',
    'int32': 'int',
    'int64': 'int',
    'bool': 'bool',
    'string': 'str',
    'bytes': 'bytes',
}

# --- SQL Parsing (Static) ---

def parse_sql_schema(sql_content):
    tables = []
    # Match CREATE TABLE blocks
    table_matches = re.finditer(r'CREATE TABLE\s+(?:public\.)?(\w+)\s*\((.*?)\);', sql_content, re.DOTALL | re.IGNORECASE)
    
    for match in table_matches:
        table_name = match.group(1)
        columns_block = match.group(2)
        
        columns = []
        lines = [l.strip() for l in columns_block.split('\n') if l.strip() and not l.strip().startswith('--')]
        
        for line in lines:
            if line.upper().startswith('CONSTRAINT'):
                continue
            
            # Match: column_name type [constraints]
            col_match = re.match(r'^"?(\w+)"?\s+([^,]+)(?:,|$)', line, re.IGNORECASE)
            if col_match:
                name = col_match.group(1)
                full_type = col_match.group(2).strip().lower()
                
                is_pk = 'PRIMARY KEY' in line.upper()
                is_nullable = 'NOT NULL' not in line.upper()
                is_autoincrement = 'SERIAL' in full_type.upper()
                
                is_array = '[]' in full_type
                base_type = full_type.replace('[]', '').split(' ')[0]
                
                columns.append({
                    'name': name,
                    'type': base_type,
                    'is_pk': is_pk,
                    'is_nullable': is_nullable,
                    'is_autoincrement': is_autoincrement,
                    'is_array': is_array
                })
        
        # Check for multi-line PK constraint
        pk_match = re.search(r'CONSTRAINT\s+\w+\s+PRIMARY KEY\s*\((.*?)\)', columns_block, re.IGNORECASE)
        if pk_match:
            pk_cols = [c.strip().strip('"') for c in pk_match.group(1).split(',')]
            for col in columns:
                if col['name'] in pk_cols:
                    col['is_pk'] = True

        tables.append({'name': table_name, 'columns': columns})
    return tables

# --- Proto Parsing (Dynamic) ---

def parse_proto(proto_content):
    messages = {}
    # Find all messages
    msg_matches = re.finditer(r'message\s+(\w+)\s*\{(.*?)\}', proto_content, re.DOTALL)
    for match in msg_matches:
        msg_name = match.group(1)
        body = match.group(2)
        fields = []
        for line in body.split('\n'):
            line = line.strip()
            if not line or line.startswith('//'): continue
            
            # Match: [repeated] type name = tag;
            field_match = re.match(r'(repeated\s+)?(\w+)\s+(\w+)\s*=\s*(\d+)\s*;', line)
            if field_match:
                is_repeated = bool(field_match.group(1))
                f_type = field_match.group(2)
                f_name = field_match.group(3)
                fields.append({'name': f_name, 'type': f_type, 'repeated': is_repeated})
        messages[msg_name] = fields
    return messages


def find_default_root_message(messages):
    for msg_name in messages.keys():
        if msg_name.endswith("SensorData"):
            return msg_name
    if messages:
        return next(iter(messages.keys()))
    return None

# --- Artifact Generation ---

def generate_prisma_from_sql(tables):
    prisma_models = []
    for table in tables:
        model_lines = [f"model {table['name']} {{"]
        model_attrs = []
        for col in table['columns']:
            prisma_type = PG_TO_PRISMA_TYPES.get(col['type'], 'String')
            if col['is_array']: prisma_type += "[]"
            
            line = f"  {col['name']} {prisma_type}"
            if col['is_pk']: line += " @id"
            if col['is_autoincrement']: line += " @default(autoincrement())"

            # Keep compatibility with existing Prisma expectations for common schema.
            if table['name'] == 'partitions' and col['name'] == 'partition_name':
                line += " @id"
            if table['name'] == 'track' and col['name'] == 'name':
                line += " @unique"
            if table['name'] == 'track' and col['name'] == 'created_at':
                line += " @default(now())"

            # Prisma does not support optional scalar lists; keep arrays non-nullable in schema.
            if col['is_nullable'] and not col['is_pk'] and not col['is_array']:
                line += "?"
            model_lines.append(line)

        # Add relation fields and model-level attributes used by viewer tool schemas.
        if table['name'] == 'lut_location':
            model_lines.append("")
            model_lines.append("  tracks track[]")

        if table['name'] == 'event':
            model_lines.append("")
            model_lines.append("  track_points track_point[]")

        if table['name'] == 'classifier':
            model_attrs.append("  @@id([event_id, type, start_time])")

        if table['name'] == 'track_point':
            model_lines.append("")
            model_lines.append("  event event @relation(fields: [event_id], references: [event_id])")
            model_attrs.append("  @@index([event_id])")

        if table['name'] == 'track':
            model_lines.append("")
            model_lines.append("  location     lut_location?  @relation(fields: [location_id], references: [location_id])")
            model_lines.append("  sector_gates sector_gate[]")
            model_attrs.append("  @@index([location_id])")

        if table['name'] == 'sector_gate':
            model_lines.append("")
            model_lines.append("  track track @relation(fields: [track_id], references: [track_id])")
            model_attrs.append("  @@unique([track_id, gate_index])")
            model_attrs.append("  @@index([track_id])")

        if model_attrs:
            model_lines.append("")
            model_lines.extend(model_attrs)

        model_lines.append("}")
        prisma_models.append("\n".join(model_lines))
    return "\n\n".join(prisma_models)

def generate_artifacts_from_proto(messages, root_msg_name):
    root_fields = messages.get(root_msg_name, [])
    sensor_tables = []
    
    # 1. Generate SQL
    sql_lines = []
    
    # Packet table (Root)
    sql_lines.append("-- Generated Packet Table")
    sql_lines.append("CREATE TABLE public.packet (")
    sql_lines.append("    packet_id           bigint   NOT NULL,")
    sql_lines.append("    \"time\"              bigint   NOT NULL,")
    sql_lines.append("    CONSTRAINT packet_pk PRIMARY KEY (packet_id)")
    sql_lines.append(");\n")
    
    for field in root_fields:
        if field['type'] in messages: # It's a sub-message
            msg_name = field['type']
            table_name = field['name']
            sql_lines.append(f"-- Generated {table_name.capitalize()} Table")
            sql_lines.append(f"CREATE TABLE public.{table_name} (")
            sql_lines.append("    packet_id           bigint   NOT NULL,")
            
            for f in messages[msg_name]:
                sql_type = PROTO_TO_SQL_TYPES.get(f['type'], 'text')
                # Convention for JSON
                if f['name'].endswith('_json'):
                    sql_type = 'jsonb'
                elif f['repeated']:
                    sql_type += '[]'
                
                # The custom GPS point was removed, so this is commented out.
                # Override for GPS convention
                # if f['name'] == 'gps' and f['type'] == 'float' and f['repeated']:
                #     sql_type = 'point'
                # elif f['repeated']:
                #     sql_type += '[]'
                
                sql_lines.append(f"    {f['name']:20} {sql_type},")
            
            sql_lines.append(f"    CONSTRAINT fk_{table_name}_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)")
            sql_lines.append(");\n")
            sensor_tables.append(table_name)

    # Indexes. `packet` is the time spine; every sensor table joins back to it
    # by packet_id. packet_id is the de-facto key of each sensor row, but the
    # DDL only carries it as a FK, so without an explicit index the latest-sample
    # and time-range queries seq-scan multi-million-row tables.
    sql_lines.append("-- Generated Indexes")
    sql_lines.append('CREATE INDEX IF NOT EXISTS idx_packet_time ON public.packet ("time" DESC);')
    for table_name in sensor_tables:
        sql_lines.append(
            f"CREATE INDEX IF NOT EXISTS idx_{table_name}_packet_id "
            f"ON public.{table_name} (packet_id);"
        )
    sql_lines.append("")

    return "\n".join(sql_lines), sensor_tables

def generate_sqlalchemy_from_proto(messages, root_msg_name, prefix=""):
    root_fields = messages.get(root_msg_name, [])
    sa_lines = []
    
    sa_lines.append(f"class {prefix}Packet(Base{prefix}):")
    sa_lines.append(f"    __tablename__ = 'packet'")
    sa_lines.append("    __table_args__ = {'schema': 'public', 'extend_existing': True}")
    sa_lines.append(f"    packet_id = Column(BigInteger, primary_key=True)")
    sa_lines.append(f"    time = Column(BigInteger, nullable=False)")
    
    for field in root_fields:
        if field['type'] in messages:
            class_name = f"{prefix}{snake_to_pascal(field['name'])}"
            sa_lines.append(f"    {field['name']} = relationship(\"{class_name}\", uselist=False, back_populates=\"packet\")")
    
    sa_lines.append("")

    for field in root_fields:
        if field['type'] in messages:
            msg_name = field['type']
            table_name = field['name']
            class_name = f"{prefix}{snake_to_pascal(table_name)}"
            
            sa_lines.append(f"class {class_name}(Base{prefix}):")
            sa_lines.append(f"    __tablename__ = '{table_name}'")
            sa_lines.append("    __table_args__ = {'schema': 'public', 'extend_existing': True}")
            sa_lines.append(f"    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)")
            
            for f in messages[msg_name]:
                sa_type = PROTO_TO_SA_TYPES.get(f['type'], 'Text')
                if f['name'].endswith('_json'):
                    sa_type = 'JSONB'
                elif f['name'] == 'gps' and f['type'] == 'float' and f['repeated']:
                    sa_type = f"ARRAY({sa_type})"
                elif f['repeated']:
                    sa_type = f"ARRAY({sa_type})"
                
                sa_lines.append(f"    {f['name']} = Column({sa_type})")
            
            sa_lines.append(f"    packet = relationship(\"{prefix}Packet\", back_populates=\"{table_name}\")")
            sa_lines.append("")
            
    return "\n".join(sa_lines)

def generate_dataclasses_from_proto(messages, root_msg_name, prefix=""):
    root_fields = messages.get(root_msg_name, [])
    py_lines = ["from dataclasses import dataclass, field", "from typing import List, Optional", ""]
    
    # Recursive generation might be better, but we only have 2 levels (root + sensors)
    # Generate sub-messages first
    for field in root_fields:
        if field['type'] in messages:
            msg_name = field['type']
            table_name = field['name']
            class_name = f"{prefix}{snake_to_pascal(table_name)}"
            
            py_lines.append("@dataclass")
            py_lines.append(f"class {class_name}:")
            for f in messages[msg_name]:
                py_type = PROTO_TO_PY_TYPES.get(f['type'], 'any')
                if f['repeated']:
                    py_type = f"List[{py_type}]"
                    py_lines.append(f"    {f['name']}: {py_type} = field(default_factory=list)")
                else:
                    py_lines.append(f"    {f['name']}: Optional[{py_type}] = None")
            py_lines.append("")

    # Generate root message
    py_lines.append("@dataclass")
    py_lines.append(f"class {prefix}SensorData:")
    py_lines.append("    packet_id: int")
    py_lines.append("    time: int")
    for field in root_fields:
        if field['type'] in messages:
            class_name = f"{prefix}{snake_to_pascal(field['name'])}"
            py_lines.append(f"    {field['name']}: Optional[{class_name}] = None")
        else:
            # Handle root-level primitives if any (though we assume packet_id/time are handled)
            if field['name'] not in ['packet_id', 'time']:
                py_type = PROTO_TO_PY_TYPES.get(field['type'], 'any')
                py_lines.append(f"    {field['name']}: Optional[{py_type}] = None")
                
    return "\n".join(py_lines)

def generate_prisma_from_proto(messages, root_msg_name):
    root_fields = messages.get(root_msg_name, [])
    prisma_lines = []
    
    prisma_lines.append("model packet {")
    prisma_lines.append("  packet_id BigInt @id")
    prisma_lines.append("  time      BigInt")
    prisma_lines.append("")
    for field in root_fields:
        if field['type'] in messages:
            prisma_lines.append(f"  {field['name']} {field['name']}?")
    prisma_lines.append("}")
    prisma_lines.append("")

    for field in root_fields:
        if field['type'] in messages:
            msg_name = field['type']
            table_name = field['name']
            prisma_lines.append(f"model {table_name} {{")
            prisma_lines.append("  packet_id BigInt @id")
            
            for f in messages[msg_name]:
                p_type = PROTO_TO_PRISMA_TYPES.get(f['type'], 'String')
                if f['name'].endswith('_json'):
                    p_type = 'Json'
                elif f['repeated']:
                    p_type += '[]'

                # Prisma does not allow optional list fields (e.g. Float[]?).
                optional_suffix = '' if f['repeated'] else '?'
                prisma_lines.append(f"  {f['name']:20} {p_type}{optional_suffix}")
            
            prisma_lines.append("")
            prisma_lines.append("  packet packet @relation(fields: [packet_id], references: [packet_id])")
            prisma_lines.append("}")
            prisma_lines.append("")
            
    return "\n".join(prisma_lines)

def concat_prisma(common_prisma_path, sensor_prisma_path, output_path, car_name):
    with open(common_prisma_path, 'r') as f: common = f.read()
    with open(sensor_prisma_path, 'r') as f: sensor = f.read()
    
    header = f"""generator client {{
  provider = "prisma-client-js"
  output   = "../.prisma/{car_name.lower()}-client"
}}

datasource db {{
  provider = "postgresql"
  url      = env("TELEMETRY_DATABASE_URL")
}}

"""
    with open(output_path, 'w') as f:
        f.write(header)
        f.write(common)
        f.write("\n\n")
        f.write(sensor)

def patch_models(models_path, patch_content, car_name):
    with open(models_path, 'r') as f: content = f.read()
    
    start_marker = f"# BEGIN GENERATED {car_name}"
    end_marker = f"# END GENERATED {car_name}"
    
    pattern = re.compile(f"{re.escape(start_marker)}.*?{re.escape(end_marker)}", re.DOTALL)
    
    if not pattern.search(content):
        content = content.rstrip() + f"\n\n# BEGIN GENERATED {car_name}\n# END GENERATED {car_name}\n"
        pattern = re.compile(f"{re.escape(start_marker)}.*?{re.escape(end_marker)}", re.DOTALL)
        
    new_section = f"{start_marker}\n{patch_content}\n{end_marker}"
    new_content = pattern.sub(new_section, content)
    
    with open(models_path, 'w') as f:
        f.write(new_content)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: generate_schema.py <command> [args]")
        sys.exit(1)
    
    cmd = sys.argv[1]
    if cmd == "sql-to-prisma":
        with open(sys.argv[2], 'r') as f: content = f.read()
        with open(sys.argv[3], 'w') as f: f.write(generate_prisma_from_sql(parse_sql_schema(content)))
    elif cmd == "proto-to-all":
        # proto-to-all <proto_file> <root_msg> <output_dir> [prefix]
        proto_file = sys.argv[2]
        root_msg = sys.argv[3]
        out_dir = sys.argv[4]
        prefix = sys.argv[5] if len(sys.argv) > 5 else ""
        
        with open(proto_file, 'r') as f: proto_content = f.read()
        messages = parse_proto(proto_content)
        if root_msg.lower() == "auto":
            resolved_root = find_default_root_message(messages)
            if not resolved_root:
                print("Error: Could not infer root message from proto")
                sys.exit(1)
            root_msg = resolved_root
        
        sql, tables = generate_artifacts_from_proto(messages, root_msg)
        sa = generate_sqlalchemy_from_proto(messages, root_msg, prefix)
        prisma = generate_prisma_from_proto(messages, root_msg)
        dc = generate_dataclasses_from_proto(messages, root_msg, prefix)
        
        os.makedirs(out_dir, exist_ok=True)
        with open(os.path.join(out_dir, "sensors.sql"), "w") as f: f.write(sql)
        with open(os.path.join(out_dir, "models.py"), "w") as f: f.write(sa)
        with open(os.path.join(out_dir, "sensors.prisma"), "w") as f: f.write(prisma)
        with open(os.path.join(out_dir, "dataclasses.py"), "w") as f: f.write(dc)
        print(f"Generated artifacts in {out_dir}")
    elif cmd == "concat-prisma":
        concat_prisma(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
    elif cmd == "patch-models":
        # patch-models <models_file> <patch_file> <car_name>
        with open(sys.argv[3], 'r') as f: patch_content = f.read()
        patch_models(sys.argv[2], patch_content, sys.argv[4])
        print(f"Patched {sys.argv[2]} for {sys.argv[4]}")