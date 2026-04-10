# Repository Guidelines

## Project Structure & Module Organization

- `include/edge_probe/`: public headers for the sender, collectors, and curl transport.
- `src/`: implementation files, including the VictoriaMetrics fixture sender in `vm_fixture_sender_main.cpp`.
- `tests/`: single custom C++ test binary in `test_main.cpp`.
- `docs/`: architecture, metrics, VictoriaMetrics smoke flow, and arm64 cross-build notes.
- `cmake/toolchains/`: cross-compilation toolchain files.
- `cmd.txt`: captured device snapshot used as the parser fixture source. Tests slice this file by line number, so changes here may require test updates.
- `build/`: generated local build output. Do not commit generated artifacts.

## Build, Test, and Development Commands

- `cmake -S . -B build`: configure the local C++17 build.
- `cmake --build build`: build the core library and `edge_probe_tests`.
- `ctest --test-dir build --output-on-failure`: run the unit suite.
- `docker build -t edge-probe-app:local .`: build the amd64 smoke image with libcurl support.
- `docker buildx build --builder multiarch --platform linux/arm64 --load -t edge-probe-app:arm64-cross -f Dockerfile.arm64-cross .`: cross-build the Debian 12 arm64 image.
- `docker run --rm --platform linux/arm64 --entrypoint /usr/local/bin/edge_probe_tests edge-probe-app:arm64-cross`: run tests in an arm64 container.

## Coding Style & Naming Conventions

- Use C++17 and follow the existing style: 4-space indentation, braces on their own line for types/functions, and short helper functions over dense inline logic.
- Prefer descriptive snake_case function names such as `parse_ip_route`; keep types in PascalCase such as `TelemetryWriter`.
- Metric names use lowercase snake case with the `edge_` prefix, for example `edge_hotspot_dhcp_leases_total`.
- No formatter config is committed; match the surrounding style when editing.

## Testing Guidelines

- Add or update coverage in `tests/test_main.cpp` for every parser or sender behavior change.
- Keep tests deterministic: use fixture slices from `cmd.txt` and fake transports/clocks instead of live network calls.
- For parser changes, verify both local tests and, when relevant, the VictoriaMetrics Docker smoke flow.

## Commit & Pull Request Guidelines

- Git history is mixed, ranging from short subjects like `auto-test` to longer descriptive summaries. Prefer concise, imperative subjects, optionally scoped, such as `Add arm64 VictoriaMetrics smoke path`.
- PRs should state what changed, how it was verified, and any doc updates. For telemetry or Docker changes, include the exact commands run and the key results (`ctest`, smoke sender output, VictoriaMetrics query/export checks).
