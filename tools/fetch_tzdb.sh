#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Fetches the tzdb release named in tools/tzdb-release.txt into a cache
# directory, outside the repository: the data archive from IANA, its
# SHA-256 checked, unpacked. A cache that already holds it is checked and
# used as it is.
#
# usage: tools/fetch_tzdb.sh <cache directory>
#   then <cache>/tzdb-<release> is the source directory mlocgen reads.
set -euo pipefail
cache=${1:?usage: fetch_tzdb.sh <cache directory>}
here=$(cd "$(dirname "$0")" && pwd)
field() { awk -v k="$1" '$1 == k { print $2 }' "$here/tzdb-release.txt"; }
release=$(field release)
sha=$(field sha256)
archive="$cache/tzdata$release.tar.gz"
mkdir -p "$cache"
if [ ! -f "$archive" ]; then
    curl -sfL -o "$archive.part" "https://data.iana.org/time-zones/releases/tzdata$release.tar.gz"
    mv "$archive.part" "$archive"
fi
if [ "$(sha256sum "$archive" | cut -c1-64)" != "$sha" ]; then
    echo "fetch_tzdb: $archive does not have the pinned SHA-256" >&2
    exit 1
fi
source="$cache/tzdb-$release"
if [ ! -f "$source/version" ]; then
    rm -rf "$source"
    mkdir -p "$source"
    tar -xzf "$archive" -C "$source"
fi
if [ "$(cat "$source/version")" != "$release" ]; then
    echo "fetch_tzdb: $source/version is not $release" >&2
    exit 1
fi
echo "fetch_tzdb: tzdb $release in $source"
