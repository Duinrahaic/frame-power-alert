#!/bin/sh
# Builds frame-power-alert and installs it as a SteamVR-bound user service. Run on the Steam Frame.
# Usage: ./install.sh [preview]   — "preview" also renders each alert into ./docs/ for the README
set -eu
cd "$(dirname "$0")"
VR=/opt/steamvr/bin/linuxarm64
BIN=~/.local/bin
UNITS=~/.config/systemd/user
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT

g++ -std=c++17 -O2 -Wall -isystem vendor -o "$BUILD/frame-power-alert" src/main.cpp -L$VR -lopenvr_api -Wl,-rpath,$VR
mkdir -p $BIN $UNITS
install -m755 "$BUILD/frame-power-alert" $BIN/frame-power-alert
install -m755 tools/steamui-patch.sh $BIN/frame-power-alert-steamui
install -m644 systemd/frame-power-alert.service systemd/frame-power-alert-steamui.service \
  systemd/frame-power-alert-steamui.path $UNITS/
units="frame-power-alert.service frame-power-alert-steamui.path"

# Auto-update needs a git checkout to pull from.
if [ -d .git ]; then
  sed "s|@REPO@|$PWD|" systemd/frame-power-alert-update.service > $UNITS/frame-power-alert-update.service
  units="$units frame-power-alert-update.service"
fi

systemctl --user daemon-reload
systemctl --user enable $units
systemctl --user restart frame-power-alert.service
systemctl --user start frame-power-alert-steamui.path frame-power-alert-steamui.service

if [ "${1:-}" = preview ]; then
  g++ -std=c++17 -O2 -Isrc -isystem vendor -o "$BUILD/preview" tools/preview.cpp
  mkdir -p docs
  "$BUILD/preview" /usr/share/fonts/noto/NotoSans-Bold.ttf docs
fi
