ARG ALPINE_VERSION=3.22
FROM alpine:${ALPINE_VERSION} AS builder

RUN apk add --no-cache \
    build-base \
    musl-dev \
    linux-headers \
    openssl-dev \
    openssl-libs-static \
    gengetopt \
    file

WORKDIR /src
COPY . .

RUN make clean || true
RUN make rctlcli STATIC=1 LTO=1 -j$(nproc)
RUN strip --strip-all client/rctlcli
RUN file client/rctlcli && ls -lh client/rctlcli

FROM scratch AS export
COPY --from=builder /src/client/rctlcli /rctlcli
