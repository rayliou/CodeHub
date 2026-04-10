# Architecture

## Goals

The current codebase is split around two responsibilities:

1. Convert raw edge-device command output into normalized metric samples.
2. Batch and deliver those samples to VictoriaMetrics.

The split is deliberate. Parsing logic changes with device behavior. Delivery logic changes with backend and operational requirements. Keeping them separate makes both parts easier to test and replace.

## Main Components

### Collector Layer

Defined in [collectors.h](../include/edge_probe/collectors.h) and implemented in [collectors.cpp](../src/collectors.cpp).

Responsibilities:

- Define `default_command_plan()` for the commands that should be run on the device.
- Parse each command output independently.
- Emit normalized `MetricSample` records with metric names, labels, and values.

Design notes:

- Parsers are pure functions over strings.
- Parsers do not execute shell commands.
- Parsers do not know anything about batching or HTTP transport.
- The test suite validates parsers against slices taken from `cmd.txt`.

### Sender Core

Defined in [telemetry_sender.h](../include/edge_probe/telemetry_sender.h) and implemented in [telemetry_sender.cpp](../src/telemetry_sender.cpp).

Responsibilities:

- Accept `MetricSample` objects through `submit()`.
- Batch samples in memory.
- Serialize the batch into VictoriaMetrics JSON-line import format.
- Send immediately when the batch is full or when the flush interval expires.
- Retry one failed payload with exponential backoff.

Design notes:

- `TelemetryWriter` depends on abstract `HttpTransport` and `Clock` interfaces.
- This keeps sender behavior unit-testable without sleeping or performing real network I/O.
- `SystemClock` is the default runtime clock implementation.

### Optional curl Transport

Defined in [curl_http_transport.h](../include/edge_probe/curl_http_transport.h) and implemented in [curl_http_transport.cpp](../src/curl_http_transport.cpp).

Responsibilities:

- Perform the actual HTTPS POST to the configured VictoriaMetrics endpoint.
- Attach basic-auth credentials.
- Set connect and request timeouts.
- Honor TLS verification settings.

This transport is optional at build time because libcurl development headers may not be present on every build host.

## Data Flow

Expected runtime flow:

1. A runner executes the command plan on the edge device.
2. The runner groups raw command output by command.
3. The runner calls the matching parser functions.
4. Parsers emit `MetricSample` records.
5. The runner passes samples into `TelemetryWriter::submit()`.
6. The runner periodically calls `TelemetryWriter::tick()`.
7. The sender serializes batches and sends them through the selected `HttpTransport`.

## Why the Sender Uses a Single Pending Payload

This V1 sender intentionally keeps only one failed payload in memory.

Tradeoff:

- Advantage: bounded memory, simpler retry state, easier operational reasoning.
- Cost: new samples are dropped while retry is pending, and unsent data is lost across process restart.

This is acceptable for the first implementation because the initial requirement was controlled behavior, not durable delivery.

## Failure Model

Current behavior on failures:

- Network or non-2xx response: current batch becomes the pending payload.
- While pending exists: `submit()` drops new samples.
- Retry delay starts at `retry_initial_ms` and doubles until `retry_max_ms`.
- HTTP `401` and `403` force a longer retry floor because they usually indicate auth or config problems.

## Test Strategy

The unit tests in [test_main.cpp](../tests/test_main.cpp) cover:

- Sender batching, retry, timestamp filling, and oversized-payload drop behavior.
- Command-plan sanity.
- Parser output for each major section present in `cmd.txt`.

The test fixture strategy is intentional:

- The repo does not duplicate large command-output blobs into many separate files.
- Tests slice the original `cmd.txt` by line range.
- This keeps the implemented metric surface tightly anchored to the original device snapshot.

## Next Recommended Layer

The next production step should be a runner executable that adds:

- Config loading for endpoint, credentials, device labels, and polling intervals.
- Command execution with timeouts.
- A scheduler or event-loop integration.
- Structured logging.
- Optional delta/rate calculations for counters that should be emitted as rates rather than raw totals.

After that, the natural reliability upgrade is a bounded in-memory queue, followed by persistent buffering such as SQLite if required.
