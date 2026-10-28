#!/bin/bash
# Removes what scripts/install-desktop.sh added (your settings, library and
# playlists in ~/.config and ~/.local/share/ChaoticMusicPlayer are kept).
set -euo pipefail

data="${XDG_DATA_HOME:-$HOME/.local/share}"
apps="$data/applications"
desktop="$apps/ChaoticMusicPlayer.desktop"
icon="$data/icons/hicolor/scalable/apps/chaoticmusicplayer.svg"

for file in "$desktop" "$icon"; do
    if [ -e "$file" ]; then
        rm -f -- "$file"
        echo "Removed $file"
    fi
done

command -v update-desktop-database >/dev/null && update-desktop-database -q "$apps" || true
