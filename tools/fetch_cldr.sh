#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Fetches the CLDR release named in tools/cldr-release.txt into a cache
# directory, outside the repository: its common/ data at the pinned
# commit (checked), and the JSON of the same release (its SHA-256
# checked). A cache that already holds them is used as it is.
#
# usage: tools/fetch_cldr.sh <cache directory>
#   then <cache>/cldr/common and <cache>/json are ready.
set -euo pipefail
cache=${1:?usage: fetch_cldr.sh <cache directory>}
here=$(cd "$(dirname "$0")" && pwd)
field() { awk -v k="$1" '$1 == k { print $2 }' "$here/cldr-release.txt"; }
tag=$(field tag)
commit=$(field commit)
json=$(field json)
json_sha=$(field json-sha256)
mkdir -p "$cache"
if [ ! -d "$cache/cldr/common" ] || [ "$(git -C "$cache/cldr" rev-parse HEAD)" != "$commit" ]; then
    rm -rf "$cache/cldr"
    git clone -q --depth 1 --branch "$tag" --filter=blob:none --sparse \
        https://github.com/unicode-org/cldr.git "$cache/cldr"
    git -C "$cache/cldr" sparse-checkout set common
fi
if [ "$(git -C "$cache/cldr" rev-parse HEAD)" != "$commit" ]; then
    echo "fetch_cldr: $tag is not at the pinned commit $commit" >&2
    exit 1
fi
zip="$cache/cldr-$json-json-full.zip"
if [ ! -f "$zip" ]; then
    curl -sfL -o "$zip.part" \
        "https://github.com/unicode-org/cldr-json/releases/download/$json/cldr-$json-json-full.zip"
    mv "$zip.part" "$zip"
fi
if [ "$(sha256sum "$zip" | cut -c1-64)" != "$json_sha" ]; then
    echo "fetch_cldr: $zip does not have the pinned SHA-256" >&2
    exit 1
fi
if [ ! -d "$cache/json/cldr-numbers-full" ]; then
    rm -rf "$cache/json"
    mkdir -p "$cache/json"
    unzip -q "$zip" -d "$cache/json"
fi
echo "fetch_cldr: $tag in $cache/cldr/common, JSON $json in $cache/json"
