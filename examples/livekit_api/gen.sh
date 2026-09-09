#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd "$script_dir/../.." && pwd)
generated_dir="$script_dir/generated"
rust_target_dir="$script_dir/target/release"

case "$(uname -s)" in
  Darwin)
    library_file="$rust_target_dir/libuniffi_cpp_livekit_api_example.dylib"
    ;;
  Linux)
    library_file="$rust_target_dir/libuniffi_cpp_livekit_api_example.so"
    ;;
  *)
    echo "Unsupported platform: $(uname -s)" >&2
    exit 1
    ;;
esac

build_rust() {
  cargo build --release --manifest-path "$script_dir/Cargo.toml"
}

generate_bindings() {
  if [[ ! -f "$library_file" ]]; then
    echo "Rust library not found. Run '$0 rust' first." >&2
    exit 1
  fi

  mkdir -p "$generated_dir"
  cargo run --release -p uniffi-bindgen-cpp --manifest-path "$repo_dir/Cargo.toml" -- \
    --library "$library_file" \
    --out-dir "$generated_dir"
}

compile_example() {
  if [[ ! -f "$generated_dir/livekit_api.cpp" ]]; then
    echo "Generated C++ bindings not found. Run '$0 bindings' first." >&2
    exit 1
  fi

  c++ -std=c++20 -I "$generated_dir" \
    "$script_dir/cpp/main.cpp" "$generated_dir/livekit_api.cpp" \
    -L "$rust_target_dir" -luniffi_cpp_livekit_api_example \
    -Wl,-rpath,"$rust_target_dir" \
    -o "$generated_dir/livekit_api-demo"
}

usage() {
  cat <<EOF
Usage: $0 [all|rust|bindings|compile]

  all       Build Rust, generate C++ bindings, and compile the demo (default).
  rust      Build the Rust library and emit UniFFI metadata.
  bindings  Generate livekit_api.hpp/livekit_api.cpp from the compiled Rust library.
  compile   Compile the generated bindings and C++ demo.
EOF
}

case "${1:-all}" in
  all)
    build_rust
    generate_bindings
    compile_example
    ;;
  rust)
    build_rust
    ;;
  bindings)
    generate_bindings
    ;;
  compile)
    compile_example
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
