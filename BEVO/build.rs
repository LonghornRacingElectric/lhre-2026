use std::env;
use std::path::PathBuf;

fn main() {
    let proto_file = env::var("PROTO_FILE").expect("PROTO_FILE env var not set");
    println!("cargo:rerun-if-changed={}", proto_file);

    let proto_path = PathBuf::from(&proto_file);
    
    let proto_dir = proto_path.parent().expect("Failed to get proto directory");

    prost_build::compile_protos(&[&proto_path], &[proto_dir])
    .expect("Failed to compile protos");
}