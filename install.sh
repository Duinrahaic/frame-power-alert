#!/bin/sh
# Builds frame-power-alert and installs it as a SteamVR-bound user service. Run on the Steam Frame.
# Usage: ./install.sh [preview]   — "preview" also renders each alert into ./docs/ for the README
set -eu
cd "$(dirname "$0")"
VR=/opt/steamvr/bin/linuxarm64

g++ -std=c++17 -O2 -Wall -isystem vendor -o frame-power-alert src/main.cpp -L$VR -lopenvr_api -Wl,-rpath,$VR
mkdir -p ~/.local/bin ~/.config/systemd/user
install -m755 frame-power-alert ~/.local/bin/frame-power-alert
install -m644 frame-power-alert.service ~/.config/systemd/user/frame-power-alert.service
systemctl --user daemon-reload
systemctl --user enable frame-power-alert.service
systemctl --user restart frame-power-alert.service

if [ "${1:-}" = preview ]; then
  g++ -std=c++17 -O2 -Isrc -isystem vendor -o preview tools/preview.cpp
  mkdir -p docs
  ./preview /usr/share/fonts/noto/NotoSans-Bold.ttf docs
  rm preview
fi
