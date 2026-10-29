#!/bin/bash
# Makes the Linux packages from a release build:
#   ChaoticMusicPlayer-<version>-x86_64.AppImage
#   ChaoticMusicPlayer-<version>-linux-x86_64.tar.gz  (install-app.sh / uninstall-app.sh inside)
# Both hold the app with Qt and every library it needs (linuxdeploy), and
# yt-dlp's standalone build. They run on systems as new as the one that built
# them (glibc), so release builds come from an older system (CI: Ubuntu 24.04).
#
# Usage: release/linux/make-packages.sh <build folder> <output folder>
set -euo pipefail

build="$(cd "$1" && pwd)"
mkdir -p "$2"
out="$(cd "$2" && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
project="$(cd "$here/../.." && pwd)"
id=io.github.chandandasgit.ChaoticMusicPlayer
version=$(sed -n 's/^project(ChaoticMusicPlayer VERSION \([0-9.]*\).*/\1/p' "$project/CMakeLists.txt")

# The tools, pinned (downloaded once into <output>/tools)
tools="$out/tools"
mkdir -p "$tools"
fetch() { [ -x "$tools/$2" ] || { curl -fsSLo "$tools/$2" "$1" && chmod +x "$tools/$2"; }; }
fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage linuxdeploy-x86_64.AppImage
fetch https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage appimagetool-x86_64.AppImage
export APPIMAGE_EXTRACT_AND_RUN=1 # the tools are AppImages too; no FUSE needed

# The AppDir: the app, its launcher entry, icon and metainfo, and yt-dlp
appdir="$out/AppDir"
rm -rf "$appdir"
install -Dm755 "$build/ChaoticMusicPlayer" "$appdir/usr/bin/ChaoticMusicPlayer"
install -Dm644 "$here/$id.desktop" "$appdir/usr/share/applications/$id.desktop"
install -Dm644 "$project/resources/icons/chaoticmusicplayer.svg" "$appdir/usr/share/icons/hicolor/scalable/apps/$id.svg"
install -Dm644 "$here/$id.metainfo.xml" "$appdir/usr/share/metainfo/$id.metainfo.xml"

# yt-dlp's standalone build, checked against its published SHA-256 sums (the app
# keeps it up to date from there; see wrapper/youtube/ytdlpmanager.h)
curl -fsSLo "$tools/yt-dlp_linux" https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux
curl -fsSLo "$tools/SHA2-256SUMS" https://github.com/yt-dlp/yt-dlp/releases/latest/download/SHA2-256SUMS
(cd "$tools" && grep ' yt-dlp_linux$' SHA2-256SUMS | sha256sum -c --quiet -)
install -Dm755 "$tools/yt-dlp_linux" "$appdir/usr/bin/yt-dlp"

# Qt and the libraries. QML_SOURCES_PATHS: where the QML imports are read from.
QML_SOURCES_PATHS="$project" QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}" \
    "$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --plugin qt \
    --executable "$appdir/usr/bin/ChaoticMusicPlayer" \
    --desktop-file "$appdir/usr/share/applications/$id.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/$id.svg"

# The AppImage
ARCH=x86_64 "$tools/appimagetool-x86_64.AppImage" --no-appstream "$appdir" "$out/ChaoticMusicPlayer-$version-x86_64.AppImage"

# The tarball: the same files, with the install scripts
name="ChaoticMusicPlayer-$version-linux-x86_64"
stage="$out/$name"
rm -rf "$stage"
mkdir -p "$stage"
cp -a "$appdir" "$stage/app"
install -m755 "$here/install-app.sh" "$here/uninstall-app.sh" "$stage/"
cp "$project/LICENSE" "$stage/"
tar -C "$out" -czf "$out/$name.tar.gz" "$name"
rm -rf "$stage"

echo "Made:"
echo "  $out/ChaoticMusicPlayer-$version-x86_64.AppImage"
echo "  $out/$name.tar.gz"
