#!/bin/sh
# Builds frame-power-alert on the Steam Frame and installs it as a SteamVR-bound user service.
# Usage: ./deploy.sh [preview]   — "preview" also renders PNGs of each state into ./preview/
set -eu
cd "$(dirname "$0")"
SSH_OPTS="-o HostName=192.168.1.115 -o HostKeyAlias=frame.tailef6c1.ts.net"
REMOTE=steamframe
DIR=frame-power-alert

ssh $SSH_OPTS $REMOTE "mkdir -p ~/$DIR/src ~/$DIR/tools ~/$DIR/vendor"
scp -q $SSH_OPTS src/* $REMOTE:$DIR/src/
scp -q $SSH_OPTS tools/* $REMOTE:$DIR/tools/
scp -q $SSH_OPTS vendor/* $REMOTE:$DIR/vendor/
scp -q $SSH_OPTS frame-power-alert.service $REMOTE:$DIR/

ssh $SSH_OPTS $REMOTE "set -e; cd ~/$DIR
  VR=/opt/steamvr/bin/linuxarm64
  g++ -std=c++17 -O2 -Wall -isystem vendor -o frame-power-alert src/main.cpp -L\$VR -lopenvr_api -Wl,-rpath,\$VR
  install -m755 frame-power-alert ~/.local/bin/frame-power-alert
  install -m644 frame-power-alert.service ~/.config/systemd/user/frame-power-alert.service
  systemctl --user daemon-reload
  systemctl --user enable frame-power-alert.service
  systemctl --user restart frame-power-alert.service"

if [ "${1:-}" = preview ]; then
  ssh $SSH_OPTS $REMOTE "set -e; cd ~/$DIR; mkdir -p preview
    g++ -std=c++17 -O2 -Isrc -isystem vendor -o preview/preview tools/preview.cpp
    preview/preview /usr/share/fonts/noto/NotoSans-Bold.ttf preview"
  mkdir -p preview
  scp -q $SSH_OPTS "$REMOTE:$DIR/preview/*.png" preview/
fi
