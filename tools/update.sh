#!/bin/sh
# Pulls the latest frame-power-alert and reinstalls it if anything changed. Run by frame-power-alert-update.service.
set -eu
cd "$(dirname "$0")/.."
before=$(git rev-parse HEAD)
timeout 60 git pull --ff-only --quiet
[ "$(git rev-parse HEAD)" = "$before" ] || exec ./install.sh
