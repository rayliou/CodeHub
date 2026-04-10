FROM debian:bookworm AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        libcurl4-openssl-dev \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build \
    && cmake --build build --target edge_probe_vm_fixture_sender edge_probe_tests

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libcurl4 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/edge-probe

COPY --from=build /src/build/edge_probe_vm_fixture_sender /usr/local/bin/edge_probe_vm_fixture_sender
COPY --from=build /src/cmd.txt /opt/edge-probe/cmd.txt

ENTRYPOINT ["/usr/local/bin/edge_probe_vm_fixture_sender"]
