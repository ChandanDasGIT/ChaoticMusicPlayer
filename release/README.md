# Releasing Tuned Chaos

Everything about packaging and releasing lives in this folder. Three things
can't: the GitHub workflows (`.github/workflows/`, where GitHub reads them),
`CMakePresets.json` (CMake looks for it at the top of the project), and the
Android media service, which is app code.

## One code base, three platforms

Code that only works on one platform sits behind a small interface with one
`.cpp` per platform, picked in `CMakeLists.txt` (`CMP_PLATFORM` is `linux`,
`windows` or `android`). Everything else is shared, so a new feature needs no
per-platform work. Every push is built for each platform by
`.github/workflows/build.yml`, so a change that breaks one shows up at once.

### Paths

Inside the app a path is a UTF-8 string with `/` between folders, on every
platform (Qt does the same). Where a `std::filesystem::path` becomes such a
string, use `generic_string()`, not `string()` (which gives `\` on Windows).
On Windows the app's manifest sets the UTF-8 code page, so `std::string`
paths are read as UTF-8 by `std::filesystem`, file streams and TagLib. Shown
to the user, a path may use the platform's own separators.

### yt-dlp

YouTube songs play through yt-dlp. At every start the app picks the newest
copy that works (its own, in its data folder; one shipped next to it; the
system's), then checks GitHub for a newer release and, if there is one,
downloads yt-dlp's standalone build for the platform, checks it against
yt-dlp's SHA-256 sums and uses it (`wrapper/youtube/ytdlpmanager.h`).
Settings > YouTube Music shows the version and has an "Update yt-dlp" button.
Build with `-DCMP_YTDLP_SELF_UPDATE=OFF` where the store updates yt-dlp and
forbids downloads at runtime (Flathub).

## Packages

| Platform | Packages |
| --- | --- |
| Linux | tarball with `install-app.sh` / `uninstall-app.sh`, AppImage, Flatpak |
| Windows | portable ZIP, installer EXE (Inno Setup) |
| Android | APK |

## Linux packages (`release/linux/`)

- `io.github.chandandasgit.TunedChaos.desktop` / `.metainfo.xml`: the
  app's launcher entry and what software centers show; every package uses
  them (the app id is also the app's desktop name).
- `make-packages.sh <build> <output>`: the AppImage and the tarball. Both hold
  the app with Qt, its libraries and yt-dlp (linuxdeploy). They run on systems
  as new as the one that built them, so CI builds them on Ubuntu 24.04.
- `install-app.sh` / `uninstall-app.sh`: inside the tarball; installs for the
  user in `~/.local/opt/TunedChaos` with a launcher entry.
- `flatpak/`: the Flatpak manifest (KDE runtime). Built in CI with Flathub's
  own tools; for Flathub, see the notes at its top.

## Publishing a release

Push a version tag: GitHub builds and tests everything, then publishes all six
packages and `SHA256SUMS.txt` by itself (`publish`, the last job of
`.github/workflows/build.yml`). Nothing is built on your computer, and a failed
build or test publishes nothing.

```sh
git tag v0.1.0-test1
git push origin master v0.1.0-test1
```

- A tag with a dash (`v0.1.0-test1`) makes a pre-release; without one
  (`v1.0.0`) a normal release.
- The tag's number must match `project(... VERSION ...)` in `CMakeLists.txt`.
  For a new version, change it there first, and raise
  `QT_ANDROID_VERSION_CODE` too (Android only updates to a higher code).
- To remove a test release and its tag:
  `gh release delete v0.1.0-test1 --cleanup-tag`

## Building locally

Linux:

```sh
cmake --preset linux-release
cmake --build --preset linux-release
./build/linux-release/TunedChaos
```
