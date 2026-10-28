# ChaoticMusicPlayer

A music player for Linux with a Qt 6 / QML interface and a pure C++ backend
(FFmpeg for decoding, PipeWire for output, TagLib for tags). It plays your
local music and can also browse and play YouTube Music.

## Features

- **Queue and playlists**: drop a folder to add its songs; sort by any column,
  or press and hold a song to move it; swipe to delete. Named playlists can be
  played without touching the Queue.
- **Local library**: Songs, Albums, Artists and Folders, refreshed in the
  background (only new or changed files are read).
- **Playback**: gapless playback, crossfade (songs that run into each other on
  an album stay gapless), shuffle and repeat, output device choice, and
  background playing (closing the window keeps the music going, if you want).
- **Lyrics**: local `.lrc` files and tags, [LRCLIB](https://lrclib.net) and
  YouTube Music; synced lyrics follow the song.
- **Album art**: embedded covers and folder images; missing ones can be found
  online (iTunes, MusicBrainz / Cover Art Archive, YouTube Music).
- **YouTube Music**: home, search, your library and saved links; songs play
  through [yt-dlp](https://github.com/yt-dlp/yt-dlp). Signing in uses the
  session of a browser you choose (the app never sees your password). Local
  songs can be matched to YouTube Music in the background for extra details.
- **Desktop integration**: MPRIS (media keys, desktop media controls,
  `playerctl`) and one running copy at a time.
- **Layout**: wide windows show the Queue, the player and a side panel next to
  each other (panels can be pinned); narrow windows show the player with
  slide-in panels.

## Requirements

- A C++20 compiler, CMake 3.21+ and pkg-config
- Qt 6.8+ (Quick, QuickControls2, DBus, Network)
- FFmpeg (libavformat, libavcodec, libavutil, libswresample)
- PipeWire (libpipewire-0.3)
- TagLib 2
- ICU, libcurl, nlohmann-json
- At run time, for YouTube playback: `yt-dlp`

Fedora:

```sh
sudo dnf install gcc-c++ cmake ninja-build pkgconf qt6-qtbase-devel qt6-qtdeclarative-devel \
    ffmpeg-free-devel pipewire-devel taglib-devel libicu-devel libcurl-devel json-devel yt-dlp
```

Arch Linux:

```sh
sudo pacman -S base-devel cmake ninja qt6-base qt6-declarative ffmpeg pipewire taglib icu curl \
    nlohmann-json yt-dlp
```

## Build and run

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ChaoticMusicPlayer
```

To add it to your app launcher (for your user only), run
`scripts/install-desktop.sh` after building; `scripts/uninstall-desktop.sh`
removes it again.

## Where it keeps your data

| What | Where |
| --- | --- |
| Settings | `~/.config/ChaoticMusicPlayer/settings.conf` |
| Queue, playlists, library index | `~/.local/share/ChaoticMusicPlayer/` |
| Found covers, lyrics and YouTube Music matches | `~/.cache/ChaoticMusicPlayer/` |

Your music files are never changed.

## Project layout

- `backend/`: plain C++ (no Qt): decoding and audio output, tags, library,
  playlists, lyrics, covers, YouTube Music. One folder per `.h` / `.cpp` pair.
- `wrapper/`: Qt objects and models that expose the backend to QML.
- `frontend/`: the QML interface.
- `scripts/`: launcher install scripts, and `shot-build.sh` / `shot-run.sh`,
  a screenshot test harness that runs the app offscreen in an isolated
  configuration.

## Note on YouTube Music

The YouTube Music features use unofficial interfaces and are meant for
personal use. They may stop working if YouTube changes them; updating yt-dlp
usually fixes playback problems.

## License

See [LICENSE](LICENSE).
