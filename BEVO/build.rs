use std::env;
use std::path::PathBuf;

fn main() {
    let proto_path = env::var("PROTO_FILE").expect("PROTO_FILE env var not set");

    let mut proto_dir = PathBuf::from(&proto_path);
    proto_dir.pop(); 

    let mut config = prost_build::Config::new();
    config.type_attribute(
        ".",
        "#[derive(serde::Serialize, serde::Deserialize)]",
    );
    config
        .compile_protos(&[proto_path], &[proto_dir])
        .unwrap();
}