#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist/debian}"

if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    echo "dpkg-buildpackage not found. Install dpkg-dev and debhelper first." >&2
    exit 1
fi

mkdir -p "${DIST_DIR}"

pushd "${ROOT_DIR}" >/dev/null
dpkg-buildpackage -us -uc -b
popd >/dev/null

find "${ROOT_DIR}/.." -maxdepth 1 -type f -name 'lzc-player_*.deb' -exec cp -f {} "${DIST_DIR}/" \;
find "${ROOT_DIR}/.." -maxdepth 1 -type f \( -name 'lzc-player_*.buildinfo' -o -name 'lzc-player_*.changes' \) -exec cp -f {} "${DIST_DIR}/" \;

echo "Generated files:"
find "${DIST_DIR}" -maxdepth 1 -type f \( -name '*.deb' -o -name '*.buildinfo' -o -name '*.changes' \) -print
