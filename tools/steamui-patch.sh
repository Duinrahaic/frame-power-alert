#!/bin/sh
# Stops Steam opening its "Slow Charger" dialog whenever the Frame switches to a low-wattage source, which pulls you out of
# VR on every battery swap. The dialog still opens from the battery entry in Quick Access.
# frame-power-alert-steamui.path re-runs this whenever Steam rewrites sp.js; it takes effect the next time Steam starts.
set -eu
SP=$HOME/.local/share/Steam/steamui/sp.js
# Minified names change between Steam builds, so match the call by shape.
CALL='[A-Za-z_$]+\.eACState==[A-Za-z_$]+\.[A-Za-z_$]+&&this\.ShowConnectedSlowDetails\(\),'

if grep -qE "$CALL" "$SP"; then
  # Steam restores the file if it was patched before: it is fighting the patch, so leave it alone.
  if cmp -s "$SP" "$SP.orig"; then
    echo "Steam restored $SP after it was patched; leaving it unpatched" >&2
    exit 1
  fi
  cp "$SP" "$SP.orig"
  # Blank the call with spaces of the same length: Steam verifies its files by size and re-downloads any that differ.
  call=$(grep -oE "$CALL" "$SP")
  blank=$(printf '%*s' ${#call} '')
  sed -i -E "s/$CALL/$blank/" "$SP"
  echo "Removed the automatic slow charger dialog from $SP"
elif grep -qF '&&this.ShowConnectedSlowDetails()' "$SP"; then
  echo "Steam's slow charger code changed; $SP was not patched" >&2
  exit 1
fi
