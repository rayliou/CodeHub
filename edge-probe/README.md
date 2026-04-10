# edge-probe

`edge-probe` is a C++ telemetry collection skeleton for an edge WiFi hotspot device.
The current implementation focuses on two concerns:

1. Parsing hotspot, LTE, network, firewall, and system-health signals from command output collected on the device.
2. Batching and sending normalized metric samples to VictoriaMetrics with a simple V1 sender model.

The repository started from the command capture in [cmd.txt](cmd.txt). That file is also used by the test suite as the source fixture for parser validation.

## Status

The repository currently contains:

- A testable telemetry sender core with in-memory batching and single-payload retry.
- A command plan that describes which device commands should be collected.
- Parser functions that convert command output into `MetricSample` objects.
- A curl-backed HTTP transport for VictoriaMetrics when libcurl development headers are available.
- A smoke executable that loads `cmd.txt` and pushes the parsed metrics to VictoriaMetrics.
- Unit tests that validate sender behavior and parser coverage against `cmd.txt`.

The repository does not yet contain:

- A production executable that runs commands on the device on a timer.
- Persistent buffering for unsent payloads.
- A scheduler or event-loop integration layer.

## Repository Layout

- [CMakeLists.txt](CMakeLists.txt): build and test targets.
- [include/edge_probe/telemetry_sender.h](include/edge_probe/telemetry_sender.h): sender interfaces and types.
- [src/telemetry_sender.cpp](src/telemetry_sender.cpp): batch, retry, and JSON-lines payload logic.
- [include/edge_probe/collectors.h](include/edge_probe/collectors.h): parser entry points and command plan.
- [src/collectors.cpp](src/collectors.cpp): parsers for the hotspot snapshot.
- [include/edge_probe/curl_http_transport.h](include/edge_probe/curl_http_transport.h): optional libcurl transport interface.
- [src/curl_http_transport.cpp](src/curl_http_transport.cpp): optional VictoriaMetrics HTTP transport.
- [tests/test_main.cpp](tests/test_main.cpp): unit tests.
- [docs/architecture.md](docs/architecture.md): component and data-flow notes.
- [docs/metrics.md](docs/metrics.md): metric inventory and command mapping.
- [docs/victoriametrics-smoke.md](docs/victoriametrics-smoke.md): Dockerized VictoriaMetrics smoke-test flow.
- [docs/arm64-cross.md](docs/arm64-cross.md): Debian 12 arm64 cross-compile and arm-container test flow.

## Build

Minimum toolchain used in this repo:

- CMake 3.20+
- C++17 compiler

Build the core library and tests:

```bash
cmake -S . -B build
cmake --build build
```

Run the tests:

```bash
ctest --test-dir build --output-on-failure
```

## libcurl Support

The sender core does not require libcurl in order to build or test.

If CMake can find libcurl, it also builds `edge_probe_curl_transport`, which posts JSON lines to VictoriaMetrics `/api/v1/import` using HTTPS basic auth.

If CMake can find libcurl, it also builds `edge_probe_vm_fixture_sender`, a small smoke executable that reads `cmd.txt`, generates metrics through the parser layer, and sends them to VictoriaMetrics.

On Debian or Ubuntu, the usual dependency is:

```bash
sudo apt-get update
sudo apt-get install -y libcurl4-openssl-dev
```

## Integration Outline

The intended runtime flow is:

1. Execute the commands listed by `default_command_plan()`.
2. Feed each command's output into the corresponding `parse_*` function.
3. Submit all resulting `MetricSample` objects into `TelemetryWriter`.
4. Call `tick()` from the process main loop.
5. Call `force_flush()` during shutdown.

A future production runner should sit above the existing libraries and own command execution, scheduling, and configuration loading.

For a real end-to-end verification flow using Dockerized VictoriaMetrics, see [docs/victoriametrics-smoke.md](docs/victoriametrics-smoke.md).

For the Debian 12 `aarch64` target-device workflow, including cross-compilation and arm64 Docker execution, see [docs/arm64-cross.md](docs/arm64-cross.md).

## Current Sender Semantics

The current sender behavior is intentionally conservative:

- In-memory batching only.
- No persistent spool.
- At most one failed payload is retained for retry.
- New samples are dropped while a pending payload is waiting to be retried.
- Retry delay uses exponential backoff capped by configuration.

This matches the V1 design target: keep behavior bounded and easy to reason about before adding queueing or persistence.

## Notes

- [cmd.txt](cmd.txt) is a captured device snapshot, not a runtime input format.
- The tests validate the parser layer against that snapshot so the current metric surface stays traceable to the original device observations.
