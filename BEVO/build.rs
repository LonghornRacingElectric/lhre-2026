use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap_or_else(|_| ".".to_string());
    let default_proto = PathBuf::from(&manifest_dir)
        .join("nonhermetic/assets/can_packets.proto")
        .to_string_lossy()
        .to_string();

    let proto_path = env::var("PROTO_FILE").unwrap_or(default_proto);

    let mut proto_dir = PathBuf::from(&proto_path);
    proto_dir.pop(); 

    let mut config = prost_build::Config::new();
    config.include_file("_.rs");
    config.type_attribute(
        ".",
        "#[derive(serde::Serialize, serde::Deserialize)]",
    );
    config
        .compile_protos(&[proto_path], &[proto_dir])
        .unwrap();
}