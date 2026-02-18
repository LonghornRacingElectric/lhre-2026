fn main() {
    let proto_file = "../../drivers/proto/template.proto";
    println!("cargo:rerun-if-changed={}", proto_file);

    prost_build::compile_protos(&[proto_file], &["../../drivers/proto/"])
        .expect("Failed to compile protos");
}
