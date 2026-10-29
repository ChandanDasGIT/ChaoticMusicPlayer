#!/bin/bash
# Installs ChaoticMusicPlayer for your user (no administrator rights needed):
#   the app in ~/.local/opt/ChaoticMusicPlayer, a launcher entry and its icon.
# Run it from the unpacked folder: ./install-app.sh. Running it again updates
# the app. Remove it with ./uninstall-app.sh (your music, settings and library stay).
set -euo pipefail

id=io.github.chandandasgit.ChaoticMusicPlayer
here="$(cd "$(dirname "$0")" && pwd)"
target="$HOME/.local/opt/ChaoticMusicPlayer"
data="${XDG_DATA_HOME:-$HOME/.local/share}"
apps="$data/applications"
icons="$data/icons/hicolor/scalable/apps"

if [ ! -x "$here/app/AppRun" ]; then
    echo "Run this from the unpacked ChaoticMusicPlayer folder (app/AppRun is missing)." >&2
    exit 1
fi

# The app (an older copy is replaced)
rm -rf "$target"
mkdir -p "$target"
cp -a "$here/app/." "$target/"

# Launcher entry and icon. Desktop files quote with "…"; inside, \ " ` $ are escaped.
quoted="\"$(printf '%s' "$target/AppRun" | sed -e 's/[\\"`$]/\\&/g')\""
sedEscape() { printf '%s' "$1" | sed -e 's/[\\&|]/\\&/g'; }
mkdir -p "$apps" "$icons"
sed -e "s|^Exec=.*|Exec=$(sedEscape "$quoted")|" "$target/usr/share/applications/$id.desktop" > "$apps/$id.desktop"
chmod 644 "$apps/$id.desktop"
install -m644 "$target/usr/share/icons/hicolor/scalable/apps/$id.svg" "$icons/$id.svg"
command -v update-desktop-database >/dev/null && update-desktop-database -q "$apps" || true

echo "ChaoticMusicPlayer is installed in $target"
echo "Start it from your app launcher, or run: \"$target/AppRun\""
