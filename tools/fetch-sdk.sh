#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
revision=a1438dff1d38bd9c65dbd693f0e5db4b9ae91779
destination=.deps/pico-sdk
if [ ! -d "$destination" ]; then
    mkdir -p .deps
    git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git "$destination"
fi
actual=$(git -C "$destination" rev-parse HEAD)
if [ "$actual" != "$revision" ] || [ -n "$(git -C "$destination" status --porcelain)" ]; then
    echo 'SDK differs from pinned 2.2.0; leaving it untouched. Set PICO_SDK_PATH to use your own SDK.' >&2
    exit 1
fi
git -C "$destination" submodule update --init lib/tinyusb
echo "Pico SDK ready: $actual"
