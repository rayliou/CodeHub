# VictoriaMetrics Smoke Test

This repository includes a small runnable executable for end-to-end verification against a real VictoriaMetrics instance:

- `edge_probe_vm_fixture_sender`

It reads [cmd.txt](../cmd.txt), converts that fixture into `MetricSample` objects, and sends them to a VictoriaMetrics `/api/v1/import` endpoint through the curl-backed transport.

## Build the App Image

```bash
docker build -t edge-probe-app:local .
```

The Docker image installs the required libcurl development package during the build stage, compiles the executable, and copies the resulting binary plus `cmd.txt` into a slim runtime image.

## Start VictoriaMetrics

```bash
docker run -d \
  --name edge-probe-vm \
  -p 8428:8428 \
  victoriametrics/victoria-metrics:latest
```

## Run the App

```bash
docker run --rm \
  --add-host=host.docker.internal:host-gateway \
  edge-probe-app:local \
  --endpoint http://host.docker.internal:8428/api/v1/import \
  --fixture /opt/edge-probe/cmd.txt \
  --device-label busstop-001
```

Expected behavior:

- The executable prints accepted sample count and sender counters.
- `sent_batches` should be greater than `0`.
- `send_failures`, `dropped_samples`, and `dropped_batches` should all be `0`.

## Verify Data Landed

Example query:

```bash
curl 'http://127.0.0.1:8428/prometheus/api/v1/query?query=edge_hotspot_dhcp_leases_total{fixture="cmd_txt",device="busstop-001"}'
```

You should see a non-empty result vector containing the imported sample.

## Cleanup

```bash
docker kill edge-probe-vm
```
