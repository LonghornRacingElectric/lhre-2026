extern crate prost_build;
use std::io::Result;

fn main() -> Result<()> {
    // This build script's purpose is to compile the protobuf file.
    // The main application logic has been moved to main.rs.
    
    // Assuming the build system (like Bazel) has set up include paths,
    // but for a cargo-centric build, we point to the proto file.
    let mut prost_build = prost_build::Config::new();
    prost_build.out_dir("src/proto"); // Output generated rust file to src/proto
    prost_build.compile_protos(&["../../drivers/proto/template.proto"], &["../../"])?;

    Ok(())
}
