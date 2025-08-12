FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git pkg-config \
    libpq-dev libssl-dev ca-certificates \
    file \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
