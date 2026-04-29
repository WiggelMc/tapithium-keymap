#!/bin/bash

set -euo pipefail

rm -rf "sources"

dump() {
    local input="$1"
    local output_dir="sources/$input"

    if [[ -z "$input" ]]; then
        echo "Usage: dump <INPUT>"
        return 1
    fi

    local store=$(nix flake archive --json \
        | jq -r ".inputs.\"$input\".path")


    mkdir -p "$output_dir"
    cp -r --no-preserve=mode,ownership "$store/." "$output_dir"
    echo "Dumped <$input>"
}

dump "glove80-zmk"
dump "zmk-mod-unicode"
