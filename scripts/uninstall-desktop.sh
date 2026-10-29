#!/bin/bash
# Removes what scripts/install-desktop.sh added (your settings, library and
# playlists in ~/.config and ~/.local/share/ChaoticMusicPlayer are kept).
set -euo pipefail

id=io.github.chandandasgit.ChaoticMusicPlayer
data="${XDG_DATA_HOME:-$HOME/.local/share}"
apps="$data/applications"
icons="$data/icons/hicolor/scalable/apps"

# (also the names from before the app had its id)
for file in "$apps/$id.desktop" "$icons/$id.svg" "$apps/ChaoticMusicPlayer.desktop" "$icons/chaoticmusicplayer.svg"; do
    if [ -e "$file" ]; then
        rm -f -- "$file"
        echo "Removed $file"
    fi
done

command -v update-desktop-database >/dev/null && update-desktop-database -q "$apps" || true
