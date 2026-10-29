#!/bin/bash
# Adds the app you built in this project to your app launcher (for your user
# only), for trying it out while developing. Packages install their own (see
# release/linux). It starts build/linux-release/ChaoticMusicPlayer or
# build/ChaoticMusicPlayer, whichever was built last; run it again if you move
# the project or switch builds. Undo with scripts/uninstall-desktop.sh.
set -euo pipefail

id=io.github.chandandasgit.ChaoticMusicPlayer
project="$(cd "$(dirname "$0")/.." && pwd)"
binary="$project/build/ChaoticMusicPlayer"
preset="$project/build/linux-release/ChaoticMusicPlayer"
if [ -x "$preset" ] && { [ ! -x "$binary" ] || [ "$preset" -nt "$binary" ]; }; then
    binary="$preset"
fi
data="${XDG_DATA_HOME:-$HOME/.local/share}"
apps="$data/applications"
icons="$data/icons/hicolor/scalable/apps"

if [ ! -x "$binary" ]; then
    echo "Build the app first: $binary is missing." >&2
    exit 1
fi

# Desktop files quote arguments with "…"; inside them \, ", ` and $ are escaped.
quoted="\"$(printf '%s' "$binary" | sed -e 's/[\\"`$]/\\&/g')\""
# (escaped for sed's replacement text: \ & and the | delimiter)
sedEscape() { printf '%s' "$1" | sed -e 's/[\\&|]/\\&/g'; }

mkdir -p "$apps" "$icons"
install -m 644 "$project/resources/icons/chaoticmusicplayer.svg" "$icons/$id.svg"
sed -e "s|^Exec=.*|Exec=$(sedEscape "$quoted")|" -e "/^Exec=/a TryExec=$(sedEscape "$binary")" \
    "$project/release/linux/$id.desktop" > "$apps/$id.desktop"
chmod 644 "$apps/$id.desktop"
# Before the app had its id these were named differently
rm -f "$apps/ChaoticMusicPlayer.desktop" "$icons/chaoticmusicplayer.svg"

# Let launchers notice. (No icon cache: the user's icon folder has no theme
# index, so launchers read it directly.)
command -v update-desktop-database >/dev/null && update-desktop-database -q "$apps" || true

echo "Installed:"
echo "  $apps/$id.desktop"
echo "  $icons/$id.svg"
