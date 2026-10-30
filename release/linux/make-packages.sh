#!/bin/bash
# Makes the Linux packages from a release build:
#   TunedChaos-<version>-x86_64.AppImage
#   TunedChaos-<version>-linux-x86_64.tar.xz  (install-app.sh / uninstall-app.sh inside)
#   SHA256SUMS.txt
# Both hold the app with Qt and the libraries it needs, and yt-dlp's standalone
# build. They run on systems as new as the one that built them (glibc), so
# release builds come from an older system (CI: Ubuntu 24.04).
# (Same approach as ChaoticNotepad's packaging/build-appimage.sh.)
#
# Usage: release/linux/make-packages.sh <build folder> <output folder>
# Needs: the Qt the build used (qmake6 / qmake on PATH, or QMAKE=/path/to/qmake), curl.
set -euo pipefail

build="$(cd "$1" && pwd)"
mkdir -p "$2"
out="$(cd "$2" && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
project="$(cd "$here/../.." && pwd)"
id=io.github.chandandasgit.TunedChaos
version=$(sed -n 's/^project(TunedChaos VERSION \([0-9.]*\).*/\1/p' "$project/CMakeLists.txt")

export ARCH=x86_64
export APPIMAGE_EXTRACT_AND_RUN=1 # the tools are AppImages too; no FUSE needed
export NO_STRIP=1                 # linuxdeploy's own strip is older than newer systems' libraries and fails on them

# ------------------------------------------------------------------ Qt
QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake || true)}"
if [ -z "$QMAKE" ]; then
    echo "qmake not found: put Qt's bin folder on PATH, or set QMAKE=/path/to/qmake" >&2
    exit 1
fi
export QMAKE
qtLibs="$("$QMAKE" -query QT_INSTALL_LIBS)"
qtPlugins="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
qtQml="$("$QMAKE" -query QT_INSTALL_QML)"
echo "Using Qt from $("$QMAKE" -query QT_INSTALL_PREFIX)"

# ------------------------------------------------------------------ the tools, pinned (downloaded once)
tools="$out/tools"
mkdir -p "$tools"
fetch() { [ -x "$tools/$2" ] || { curl -fsSL --retry 3 -o "$tools/$2" "$1" && chmod +x "$tools/$2"; }; }
fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage linuxdeploy-x86_64.AppImage
fetch https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage appimagetool-x86_64.AppImage
export PATH="$tools:$PATH" # linuxdeploy finds its Qt plugin here

# ------------------------------------------------------------------ the AppDir: the app, as installed
appdir="$out/AppDir"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr
desktop="$appdir/usr/share/applications/$id.desktop"
icon="$appdir/usr/share/icons/hicolor/scalable/apps/$id.svg"
for f in "$appdir/usr/bin/TunedChaos" "$desktop" "$icon"; do
    [ -e "$f" ] || { echo "Missing after install: $f" >&2; exit 1; }
done

# yt-dlp's standalone build, checked against its published SHA-256 sums (the app
# keeps it up to date from there; see wrapper/youtube/ytdlpmanager.h)
curl -fsSL --retry 3 -o "$tools/yt-dlp_linux" https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux
curl -fsSL --retry 3 -o "$tools/SHA2-256SUMS" https://github.com/yt-dlp/yt-dlp/releases/latest/download/SHA2-256SUMS
(cd "$tools" && grep ' yt-dlp_linux$' SHA2-256SUMS | sha256sum -c --quiet -)
install -Dm755 "$tools/yt-dlp_linux" "$appdir/usr/bin/yt-dlp"

# ------------------------------------------------------------------ Qt and the libraries (linuxdeploy)
# Only the app's QML is scanned for imports (not build folders)
qmlSources="$out/qml-sources"
rm -rf "$qmlSources"
mkdir -p "$qmlSources"
cp "$project/Main.qml" "$qmlSources/"
cp -r "$project/frontend" "$qmlSources/"
export QML_SOURCES_PATHS="$qmlSources"

# Wayland: Qt 6.10+ has one platform plugin (libqwayland.so), older Qt two
platforms=""
for p in libqwayland.so libqwayland-generic.so libqwayland-egl.so; do
    [ -f "$qtPlugins/platforms/$p" ] && platforms="${platforms:+$platforms;}$p"
done
export EXTRA_PLATFORM_PLUGINS="$platforms"
extra=""
for d in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
    [ -d "$qtPlugins/$d" ] && extra="${extra:+$extra;}$d"
done
export EXTRA_QT_PLUGINS="$extra"

linuxdeploy-x86_64.AppImage --appdir "$appdir" --plugin qt \
    --executable "$appdir/usr/bin/TunedChaos" --desktop-file "$desktop" --icon-file "$icon"

# Wayland's OpenGL plugin: linuxdeploy doesn't pick it up, and without it the
# app can't draw under Wayland
gfx=wayland-graphics-integration-client
if [ -d "$qtPlugins/$gfx" ]; then
    mkdir -p "$appdir/usr/plugins/$gfx"
    for f in "$qtPlugins/$gfx"/*.so; do
        [ -e "$f" ] || continue
        case "$f" in
            *-server.so) ;; # (server side: not needed by an app)
            *) cp -L "$f" "$appdir/usr/plugins/$gfx/" ;;
        esac
    done
fi
for lib in libQt6WaylandClient.so.6 libQt6WaylandEglClientHwIntegration.so.6 libQt6WlShellIntegration.so.6; do
    if [ ! -e "$appdir/usr/lib/$lib" ] && [ -e "$qtLibs/$lib" ]; then cp -L "$qtLibs/$lib" "$appdir/usr/lib/"; fi
done

# ------------------------------------------------------------------ untouched libraries, plugins, QML modules
# linuxdeploy rewrites the headers of everything it bundles (where to look for
# libraries); on newer systems that damages some and the app crashes as they
# load (on Fedora 44: inside libsystemd). So each is put back exactly as it is
# in Qt or on this system; AppRun (below) sets the library paths instead.
originalLib() { # <file name> -> the untouched 64-bit library, or nothing
    local src="$qtLibs/$1"
    if [ ! -e "$src" ]; then
        src="$( { ldconfig -p 2>/dev/null || /sbin/ldconfig -p 2>/dev/null || true; } \
                | awk -v l="$1" '$1 == l && /x86-64/ && !found { print $NF; found = 1 }' )"
    fi
    printf '%s' "$src"
}
while IFS= read -r -d '' f; do
    src="$(originalLib "$(basename "$f")")"
    if [ -n "$src" ] && [ -e "$src" ]; then cp -L --remove-destination "$src" "$f"; fi
done < <(find "$appdir/usr/lib" -maxdepth 1 \( -type f -o -type l \) -name '*.so*' -print0)

restoreTree() { # <folder in the AppDir> <the same folder in Qt>
    local f rel
    [ -d "$1" ] || return 0
    while IFS= read -r -d '' f; do
        rel="${f#"$1"/}"
        if [ -f "$2/$rel" ]; then cp -L --remove-destination "$2/$rel" "$f"; fi
    done < <(find "$1" -type f -name '*.so*' -print0)
}
restoreTree "$appdir/usr/plugins" "$qtPlugins"
restoreTree "$appdir/usr/qml" "$qtQml"

# Left out: libraries that must be the system's own, matching its services
# (systemd, the sound server and its plugins); every desktop Linux has them.
for library in libsystemd.so libudev.so libpipewire-0.3.so libpulse libasound.so libjack.so; do
    rm -f "$appdir"/usr/lib/"$library"*
done

# AppRun: points the app at the bundled libraries, plugins and QML modules
rm -f "$appdir/AppRun"
cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QML_IMPORT_PATH="$HERE/usr/qml"
export QML2_IMPORT_PATH="$HERE/usr/qml"
exec "$HERE/usr/bin/TunedChaos" "$@"
EOF
chmod +x "$appdir/AppRun"

# ------------------------------------------------------------------ the AppImage
appImage="TunedChaos-$version-x86_64.AppImage"
appimagetool-x86_64.AppImage --no-appstream "$appdir" "$out/$appImage"

# ------------------------------------------------------------------ the tarball: the same files, with the install scripts
name="TunedChaos-$version-linux-x86_64"
stage="$out/$name"
rm -rf "$stage"
mkdir -p "$stage"
cp -a "$appdir" "$stage/app"
install -m755 "$here/install-app.sh" "$here/uninstall-app.sh" "$stage/"
cp "$project/LICENSE" "$stage/"
XZ_OPT=-T0 tar -C "$out" -cJf "$out/$name.tar.xz" "$name"
rm -rf "$stage" "$qmlSources"

# ------------------------------------------------------------------ checksums
(cd "$out" && sha256sum -- "$appImage" "$name.tar.xz" > SHA256SUMS.txt)

echo "Made, in $out:"
echo "  $appImage"
echo "  $name.tar.xz"
echo "  SHA256SUMS.txt"
