# LiveKit integration notes

This branch is a focused evaluation fork for replacing LiveKit's protobuf request/response FFI
boundary with generated UniFFI C++ bindings. It targets UniFFI 0.31.2 and supports both UDL and
proc-macro library metadata.

## What the fork adds

- UniFFI 0.31.2 metadata and ABI compatibility.
- Proc-macro exports discovered with `--library`, matching `livekit-uniffi`'s Rust annotations.
- Async namespace functions, constructors, and object methods.
- A move-only `uniffi::Future<T>` with explicit and destructor-driven cancellation.
- Typed UniFFI errors and Rust panic propagation through `Future::get()`.
- UniFFI 0.31 trait-interface handle tagging, clone/free lifecycle, and Rust/foreign round trips.

Async trait/callback-interface methods are still rejected during generation. Synchronous callback
interfaces are supported.

## Build and generate

Build the Rust library as a `cdylib` or `staticlib`, then generate from the compiled library. Library
mode is required to discover proc-macro exports; a UDL file alone cannot describe them.

```bash
cargo build -p livekit-uniffi
cargo run -p uniffi-bindgen-cpp -- \
  --library path/to/liblivekit_uniffi.dylib \
  --out-dir generated
```

Compile each generated `<namespace>.cpp`, include `<namespace>.hpp`, link the Rust library, and use
C++17 or newer.

## Async C++ shape

Given a Rust export such as:

```rust
#[uniffi::export]
pub async fn connect(url: String, token: String) -> Result<Arc<Room>, ConnectError> {
    // ...
}
```

the generated C++ call has this shape:

```cpp
auto connect = livekit::connect(url, token);

try {
    std::shared_ptr<livekit::Room> room = connect.get();
} catch (const livekit::connect_error::InvalidToken& error) {
    // Typed Rust error.
} catch (const std::runtime_error& panic) {
    // Rust panic or unexpected callback failure.
}
```

Cancellation is explicit and also happens when an incomplete future is destroyed:

```cpp
auto connect = livekit::connect(url, token);
connect.cancel();

try {
    connect.get();
} catch (const uniffi::AsyncCancelledError&) {
    // Expected cancellation.
}
```

UniFFI continuations are deferred onto a bounded, single-worker dispatcher by default to avoid
re-entering Rust while its future scheduler lock is held. LiveKit can register its task queue with
`uniffi::set_async_dispatcher(dispatch, shutdown)`. The dispatch callback reports whether work was
accepted; rejection fails and frees the Rust future deterministically. The shutdown callback must
drain accepted work before returning. Call `uniffi::shutdown_async_dispatcher()` before unloading
the executor or generated binding code.

## Recommended migration sequence

1. Keep the existing protobuf FFI as the shipping path and add generated bindings to a separate
   experimental target.
2. Export one narrow lifecycle slice from `livekit-uniffi` (for example room construction,
   connect/disconnect, and a small event callback interface).
3. Bridge `uniffi::Future<T>` into the SDK's public future/task abstraction. Do not expose the
   generated future in LiveKit's stable public API yet.
4. Validate cancellation during shutdown, object destruction on both sides, callbacks from Rust
   threads, and typed errors before migrating additional methods.
5. Replace protobuf payloads incrementally. Keep protobuf only where schema compatibility or
   cross-process transport is independently valuable.

## Remaining production work

- Register and exercise LiveKit's executor through the generated dispatcher hook.
- Adapt `uniffi::ForeignFuture<T>` callback implementations to LiveKit's executor and cancellation
  primitives; the generated adapter does not create worker threads for foreign callbacks.
- Exercise the generated API against the actual `livekit-uniffi` library on every supported OS and
  architecture.
- Decide and document ABI/version pinning. The generator and Rust crate must use the same UniFFI
  version.
- Add packaging support so generated sources and the Rust static/dynamic library are rebuilt and
  distributed together.

## Repository verification

Run:

```bash
cargo check --workspace
./test_bindings.sh
```

The fixture suite includes async success, typed error, explicit cancellation, cancellation on
destruction, callback lifecycle, and Rust/foreign trait-object round trips.
