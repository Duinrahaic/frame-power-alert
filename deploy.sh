#!/bin/sh
# Copies the project to a Steam Frame over SSH and runs install.sh there.
# Usage: ./deploy.sh [preview]   — "preview" also copies the rendered alerts back into ./docs/
# FRAME is the SSH destination (default "steamframe"); FRAME_SSH_OPTS adds options to every ssh/scp call.
set -eu
cd "$(dirname "$0")"
SSH_OPTS="${FRAME_SSH_OPTS:-}"
REMOTE="${FRAME:-steamframe}"
DIR=frame-power-alert

ssh $SSH_OPTS $REMOTE "rm -rf ~/$DIR && mkdir -p ~/$DIR"
scp -q -r $SSH_OPTS src tools vendor systemd install.sh $REMOTE:$DIR/
ssh $SSH_OPTS $REMOTE "sh ~/$DIR/install.sh ${1:-}"

if [ "${1:-}" = preview ]; then
  mkdir -p docs
  scp -q $SSH_OPTS "$REMOTE:$DIR/docs/*.png" docs/
fi
