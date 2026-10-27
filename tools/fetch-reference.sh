#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
revision=6c3c08b69d8d98d8b09462eb7285be20f84d8a20
destination=.deps/arduino-SAM
if [ ! -d "$destination" ]; then
    mkdir -p .deps
    git clone --no-checkout https://github.com/pschatzmann/arduino-SAM.git "$destination"
    git -C "$destination" checkout --detach "$revision"
fi
actual=$(git -C "$destination" rev-parse HEAD)
if [ "$actual" != "$revision" ] || [ -n "$(git -C "$destination" status --porcelain)" ]; then
    echo 'Reference checkout differs from the pinned revision; leaving it untouched.' >&2
    exit 1
fi
echo "Reference ready: $actual"
