# Releasing ChaoticMusicPlayer

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

## Packages (filled in step by step)

| Platform | Packages |
| --- | --- |
| Linux | tarball with `install-app.sh` / `uninstall-app.sh`, AppImage, Flatpak |
| Windows | portable ZIP, installer EXE (NSIS), MSI |
| Android | APK |

## Building locally

Linux:

```sh
cmake --preset linux-release
cmake --build --preset linux-release
./build/linux-release/ChaoticMusicPlayer
```
