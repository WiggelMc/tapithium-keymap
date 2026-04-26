#!/bin/bash

set -euo pipefail

FLASH_FILE="result/glove80.uf2"

if [ ! -f "$FLASH_FILE" ]; then
    echo "File not found: $FLASH_FILE"
    exit 1
fi

flash() {
    local label="$1"
    local mountname="$2"
    local name="$3"
    local mountdir="flash/$mountname"

    if [[ -z "$label" || -z "$mountname" || -z "$name" ]]; then
        echo "Usage: flash <LABEL> <MOUNTNAME> <NAME>"
        return 1
    fi

    echo "Waiting: $name"

    while ! blkid -L "$label" >/dev/null 2>&1; do
        sleep 1
    done
    sleep 1

    echo "Mounting: $name"

    mkdir -p "$mountdir"
    sudo mount -L "$label" "$mountdir"

    echo "Flashing: $name"
    sudo cp "$FLASH_FILE" "$mountdir/"

    sudo umount "$mountdir"
    echo "Flashed: $name"
}

flash "GLV80LHBOOT" "glv80lhboot" "Left"
flash "GLV80RHBOOT" "glv80rhboot" "Right"

