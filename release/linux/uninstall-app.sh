#!/bin/bash
# Removes what install-app.sh installed. Your music, settings, library and
# playlists (~/.config and ~/.local/share/ChaoticMusicPlayer) are kept.
set -euo pipefail

id=io.github.chandandasgit.ChaoticMusicPlayer
data="${XDG_DATA_HOME:-$HOME/.local/share}"
target="$HOME/.local/opt/ChaoticMusicPlayer"

for file in "$data/applications/$id.desktop" "$data/icons/hicolor/scalable/apps/$id.svg" "$target"; do
    if [ -e "$file" ]; then
        rm -rf -- "$file"
        echo "Removed $file"
    fi
done
command -v update-desktop-database >/dev/null && update-desktop-database -q "$data/applications" || true
echo "Your settings and library are still in ~/.config and ~/.local/share/ChaoticMusicPlayer."
