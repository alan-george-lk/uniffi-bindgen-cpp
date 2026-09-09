# Basic LiveKit-shaped procedural-macro example

This example uses Nord bindgen's library mode with UniFFI `0.31.2`, matching
this repository's generator.
The API models an in-memory `Room`: connection state, connection options,
room snapshots, and asynchronous `connect`/`send_data` calls. It is deliberately
free of signaling, media, and callbacks so the generated C++ async/object flow
is easy to inspect.

The API is declared in Rust with `#[derive(uniffi::Record)]`,
`#[derive(uniffi::Enum)]`, `#[derive(uniffi::Object)]`, and `#[uniffi::export]`.

From the repository root, build the Rust library, generate C++ bindings, and
compile the demo in one command:

```bash
./examples/livekit_api/gen.sh
```

Run an individual step when iterating:

```bash
./examples/livekit_api/gen.sh rust
./examples/livekit_api/gen.sh bindings
./examples/livekit_api/gen.sh compile
```

The command writes `livekit_api.hpp`, `livekit_api.cpp`, and
`livekit_api_scaffolding.hpp`.
After the default command or `compile`, run the small smoke test:

```bash
examples/livekit_api/generated/livekit_api-demo
```
