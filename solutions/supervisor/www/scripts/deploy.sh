#!/usr/bin/env bash
set -euo pipefail

# Deploy the supervisor web app to a reCamera device.
# Usage: ./scripts/deploy.sh [host] [user]

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST="${1:-192.168.118.90}"
USER="${2:-recamera}"
PORT="${SSH_PORT:-22}"
REMOTE_TMP_DIR="${REMOTE_TMP_DIR:-/home/${USER}/dist}"
REMOTE_WWW_DIR="${REMOTE_WWW_DIR:-/usr/share/supervisor/www}"
BUILD_CMD="${BUILD_CMD:-npm run build}"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"

echo "Building in ${ROOT_DIR}..."
(cd "${ROOT_DIR}" && ${BUILD_CMD})

if [[ ! -d "${DIST_DIR}" ]]; then
  echo "Dist directory not found: ${DIST_DIR}"
  exit 1
fi

echo "Uploading dist to ${USER}@${HOST}:${REMOTE_TMP_DIR}..."
ssh -p "${PORT}" "${USER}@${HOST}" "mkdir -p '${REMOTE_TMP_DIR}'"
scp -P "${PORT}" -r "${DIST_DIR}"/* "${USER}@${HOST}:${REMOTE_TMP_DIR}/"

echo "Replacing ${REMOTE_WWW_DIR} on device..."
ssh -t -p "${PORT}" "${USER}@${HOST}" \
  "sudo rm -rf '${REMOTE_WWW_DIR}' && sudo mkdir -p '${REMOTE_WWW_DIR}' && sudo cp -r '${REMOTE_TMP_DIR}'/* '${REMOTE_WWW_DIR}'"

echo "Rebooting device..."
ssh -t -p "${PORT}" "${USER}@${HOST}" "sudo reboot"

echo "Done."
