# Debian 12 arm64 Cross-Compile and Docker Test

The target device is Debian 12 on `aarch64`:

- `PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"`
- `Linux hotspot ... aarch64 GNU/Linux`

This repository includes an explicit cross-compile flow for that target.

## What This Path Does

1. Uses an amd64 Debian build container.
2. Installs the `aarch64-linux-gnu` cross compiler and arm64 libcurl development package.
3. Cross-compiles:
   - `edge_probe_vm_fixture_sender`
   - `edge_probe_tests`
4. Produces an arm64 Debian 12 runtime image.
5. Runs the arm64 binaries inside Docker using `--platform linux/arm64`.

## Toolchain File

The cross-toolchain file is:

- [cmake/toolchains/aarch64-linux-gnu.cmake](../cmake/toolchains/aarch64-linux-gnu.cmake)

## Build the arm64 Image

```bash
docker buildx build \
  --platform linux/arm64 \
  --load \
  -t edge-probe-app:arm64-cross \
  -f Dockerfile.arm64-cross .
```

This build uses cross-compilation in the build stage. It does not rely on native arm64 compilation.

## Enable arm64 Container Execution on amd64 Hosts

If the Docker host cannot run `linux/arm64` containers yet, install `binfmt/qemu` support first:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

Quick verification:

```bash
docker run --rm --platform linux/arm64 debian:bookworm uname -m
```

Expected output:

```text
aarch64
```

## Run the arm64 Unit Tests

```bash
docker run --rm \
  --platform linux/arm64 \
  --entrypoint /usr/local/bin/edge_probe_tests \
  edge-probe-app:arm64-cross
```

## Run the arm64 Smoke Sender Against VictoriaMetrics

Start VictoriaMetrics:

```bash
docker run -d \
  --rm \
  --name edge-probe-vm \
  -p 8428:8428 \
  victoriametrics/victoria-metrics:latest
```

Run the arm64 sender container:

```bash
docker run --rm \
  --platform linux/arm64 \
  --add-host=host.docker.internal:host-gateway \
  edge-probe-app:arm64-cross \
  --endpoint http://host.docker.internal:8428/api/v1/import \
  --fixture /opt/edge-probe/cmd.txt \
  --device-label busstop-001
```

Expected sender result:

- `accepted_samples=534`
- `total_samples=534`
- `sent_batches=1`
- `send_failures=0`
- `dropped_samples=0`
- `dropped_batches=0`

Verify an imported metric:

```bash
curl --get http://127.0.0.1:8428/prometheus/api/v1/query \
  --data-urlencode 'query=edge_hotspot_dhcp_leases_total{fixture="cmd_txt",device="busstop-001"}'
```

## Notes

- Running arm64 containers on an amd64 Docker host requires `binfmt/qemu` support.
- If `docker run --platform linux/arm64 ...` fails with `exec format error`, install arm64 emulation first.
