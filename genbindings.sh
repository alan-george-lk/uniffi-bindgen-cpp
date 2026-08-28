cargo build --release --manifest-path examples/logger/Cargo.toml && \

cargo run --release -p uniffi-bindgen-cpp -- \
  --library examples/logger/target/release/libuniffi_cpp_logger_example.dylib \
  --out-dir examples/logger/generated