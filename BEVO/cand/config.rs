use serde::Deserialize;

#[derive(Debug, Deserialize, Clone)]
pub struct PacketConfig {
    pub packet_id: u32,
    pub packet_name: String,
    pub bytes: Vec<SignalConfig>,
}

#[derive(Debug, Deserialize, Clone)]
pub struct SignalConfig {
    pub start_byte: usize,
    pub length: usize,
    pub name: String,
    pub conv_type: String, // "uint16", "int16", "bitfield", etc.
    pub precision: f64,
    pub bitfield_encoding: Option<Vec<BitfieldMapping>>,
    pub protobuf: Option<ProtobufMapping>,
}

#[derive(Debug, Deserialize, Clone)]
pub struct BitfieldMapping {
    pub protobuf_field: String,
    pub bit_index: u8,
}

#[derive(Debug, Deserialize, Clone)]
pub struct ProtobufMapping {
    #[serde(rename = "field")]
    pub field_name: String,
    #[serde(default)]
    pub repeated: bool,
    pub field_index: Option<usize>,
}