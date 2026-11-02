#!/bin/sh
# Installs the latest local build in /Applications as a brand new user would
# get it: no previous app copy, no settings, no cached Dock icon.
# Usage: herramientas/instalar-mac-desde-cero.sh [ruta/a/WorshipStudio.app]
set -e
BUILD="${1:-$HOME/WorshipStudio-build/frontend/RelWithDebInfo/WorshipStudio.app}"
HERE="$(cd "$(dirname "$0")" && pwd)"

if pgrep -f "/WorshipStudio.app/Contents/MacOS/WorshipStudio" >/dev/null; then
  echo "WorshipStudio está abierto: cerralo (Cmd+Q) y volvé a correr esto." >&2
  exit 1
fi

python3 -I "$HERE/limpiar-marca.py" "$BUILD" >/dev/null
rm -rf /Applications/WorshipStudio.app "$HOME/Applications/WorshipStudio.app"
ditto "$BUILD" /Applications/WorshipStudio.app
codesign --force --deep -s - /Applications/WorshipStudio.app 2>/dev/null
rm -rf "$HOME/Library/Application Support/WorshipStudio"
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f /Applications/WorshipStudio.app
killall Dock 2>/dev/null || true
echo "Listo: abrí WorshipStudio desde Aplicaciones."
