#!/usr/bin/env bash
set -euo pipefail

readonly SKY_URL="https://svs.gsfc.nasa.gov/vis/a000000/a004800/a004851/starmap_2020_8k.exr"
readonly SKY_SHA256="dc6c4f413e85707a29a25a9451148154554ecca2c996f84fa8f47b65ef9ff7c4"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPOSITORY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly DESTINATION="${REPOSITORY_ROOT}/assets/sky/starmap_2020_8k.exr"

if ! command -v curl >/dev/null 2>&1; then
  echo "curl is required to download the NASA sky map." >&2
  exit 1
fi

mkdir -p "$(dirname "${DESTINATION}")"

if [[ -f "${DESTINATION}" ]]; then
  if printf '%s  %s\n' "${SKY_SHA256}" "${DESTINATION}" | sha256sum --check --status; then
    echo "NASA 8K star map is already installed: ${DESTINATION}"
    exit 0
  fi
  echo "Existing sky map has the wrong checksum; downloading a clean copy." >&2
fi

temporary_file="$(mktemp "${DESTINATION}.download.XXXXXX")"
trap 'rm -f "${temporary_file}"' EXIT

echo "Downloading NASA Deep Star Maps 2020 (8K OpenEXR, about 125 MB)..."
curl --fail --location --show-error --progress-bar \
  --output "${temporary_file}" "${SKY_URL}"

printf '%s  %s\n' "${SKY_SHA256}" "${temporary_file}" | sha256sum --check --status || {
  echo "Downloaded sky map failed its SHA-256 checksum." >&2
  exit 1
}

mv "${temporary_file}" "${DESTINATION}"
trap - EXIT
echo "Installed NASA star map at ${DESTINATION}"
