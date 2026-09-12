ARG ALPINE_VERSION=3.22
FROM alpine:${ALPINE_VERSION} AS builder

RUN apk add --no-cache \
    build-base \
    musl-dev \
    linux-headers \
    openssl-dev \
    openssl-libs-static \
    readline-dev \
    readline-static \
    ncurses-dev \
    ncurses-static \
    gengetopt \
    file

WORKDIR /src
COPY . .

RUN make clean || true
RUN make all STATIC=1 LTO=1 -j$(nproc)
RUN strip --strip-all client/rctlcli server/rctlser
RUN file client/rctlcli server/rctlser && ls -lh client/rctlcli server/rctlser

FROM scratch AS export
COPY --from=builder /src/client/rctlcli /rctlcli
COPY --from=builder /src/server/rctlser /rctlser
