# cFS + ESP32 bridge, built and run in a container.
#
# Stage 1 builds cFS exactly like build.sh does (native_std).
# Stage 2 keeps only what's needed to run: core-cpu1 and the cf/ folder
# (app .so files, tables, startup script).

FROM ubuntu:22.04 AS build

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake && \
    rm -rf /var/lib/apt/lists/*

# Bundled SBN apps declare cmake_minimum_required < 3.5 (needed for CMake 4)
ENV CMAKE_POLICY_VERSION_MINIMUM=3.5

WORKDIR /src
COPY . .
RUN make native_std.prep && make -j"$(nproc)" native_std.install


FROM ubuntu:22.04

WORKDIR /cfs
COPY --from=build /src/build-native_std/exe/cpu1/core-cpu1 ./
COPY --from=build /src/build-native_std/exe/cpu1/cf ./cf

# CI_LAB listens for commands on UDP 1234; TO_LAB sends telemetry to the ground (sensor GUI: 2234)
EXPOSE 1234/udp

CMD ["./core-cpu1"]
