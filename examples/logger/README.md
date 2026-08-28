# Basic procedural-macro example

This example uses Nord bindgen's library mode with UniFFI `0.29.4`, matching
this repository's generator.
The API is declared in Rust with `#[derive(uniffi::Record)]`,
`#[derive(uniffi::Enum)]`, and `#[uniffi::export]`; `src/basic.udl` remains as
a UDL comparison reference and is not used by this build.

Build the Rust library first. The macros emit UniFFI metadata into it:

```bash
cargo build --release --manifest-path examples/logger/Cargo.toml
```

Generate C++ from the compiled library, not from the UDL file:

```bash
cargo run --release -p uniffi-bindgen-cpp -- \
  --library examples/logger/target/release/libuniffi_cpp_logger_example.dylib \
  --out-dir examples/logger/generated
```

On Linux, use `libuniffi_cpp_logger_example.so` instead of `.dylib`.

The command writes `logger.hpp`, `logger.cpp`, and `logger_scaffolding.hpp`.
Compile `logger.cpp` into the C++ target, link the Rust library, then run the
small smoke test:

```bash
c++ -std=c++20 -I examples/logger/generated \
  examples/logger/cpp/main.cpp examples/logger/generated/logger.cpp \
  -L examples/logger/target/release -luniffi_cpp_logger_example \
  -Wl,-rpath,"$PWD/examples/logger/target/release" \
  -o examples/logger/generated/logger-demo
examples/logger/generated/logger-demo
```
