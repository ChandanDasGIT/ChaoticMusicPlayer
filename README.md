# ChaoticMusicPlayer

A music player for Linux with a Qt 6 / QML interface and a pure C++ backend
(FFmpeg for decoding, PipeWire for output, TagLib for tags). It plays your
local music and can also browse and play YouTube Music.

## Features

### Playing music

- **Play from where you are**: clicking a song in the Library plays the list
  it's in (all songs, an album, an artist, a folder) from that song, with
  next / previous. Nothing is added to the Queue unless you add it.
- **Queue**: your own list. Add songs with *Add to queue* or *Play next* in
  the song menus, by dragging songs, albums, folders or YouTube playlists onto
  it (dropped at the spot you choose), or by dropping files and folders on the
  window. Sort by any column, press and hold a song to move it, swipe to
  delete.
- **Playlists**: named playlists that play without touching the Queue;
  import and export them as M3U / M3U8 (or drop an `.m3u` file on the window).
- **Between songs**: gapless playback, or a fade: each song fades out at its
  very end and the next one fades in from its start, so nothing overlaps or
  is cut. Songs that run into each other on an album always stay gapless.
- **10-band equalizer** (31 Hz to 16 kHz, ±12 dB) with presets, heard as you
  move the sliders; boosting keeps the volume (a limiter only catches peaks
  that would distort). Open it from the button beside the volume bar.
- **Resume where you left off**: the song, the position and the list it was
  playing from come back, paused, when you start the app again.
- Shuffle and repeat, output device choice, a quality chip (sample rate,
  bitrate, format), and background playing (closing the window keeps the
  music going, if you want).

### Library

- Songs, Albums, Artists and Folders, with live search, refreshed in the
  background (only new or changed files are read).
- **Folders** is browsed like a file manager: a path bar, then the folder's
  subfolders and songs; play or queue a whole folder, subfolders included.
- The music folders (and the ones to leave out) are chosen in Settings.

### YouTube Music

- Home, search, your library (liked songs) and saved links; songs play
  through [yt-dlp](https://github.com/yt-dlp/yt-dlp).
- **Mix (autoplay)**: a song plays followed by what YouTube Music plays next,
  topped up as it goes; an album or playlist plays as it is, then its Mix.
  When the Queue ends on a YouTube song, its Mix can follow. Both can be
  turned off.
- **Recent searches**: the searches you used, newest first; remove one or all.
- **Audio quality**: Auto (best, but the smallest streams on a metered
  connection), Best or Lowest.
- Signing in uses the session of a browser you choose (the app never sees
  your password). Local songs can be matched to YouTube Music in the
  background for extra details.

### Lyrics and covers

- **Lyrics**: local `.lrc` files and tags, [LRCLIB](https://lrclib.net) and
  YouTube Music, in the order you choose; synced lyrics follow the song.
- **Album art**: embedded covers and folder images; missing ones can be found
  online (iTunes, MusicBrainz / Cover Art Archive, YouTube Music).

### Look and layout

- The layout follows how much of the screen the window covers, on any
  monitor:
  - **full screen**: the Queue, the player and YouTube Music or the Library
    side by side, sharing the width equally;
  - **half screen**: two columns at most; an opened panel can push the player
    aside, and a floating mini player stands in for it. Panels can be swiped
    away, or swiped in from the window's edge;
  - **phone size**: the player, with panels sliding over it.
  Pinned panels stay open (also after a restart).
- Theme: follow the system, dark or light. The background takes on the
  colour of the playing song's cover. Optional visualizer.

### Desktop integration

- MPRIS (media keys, desktop media controls, `playerctl`) and one running copy
  at a time.

## Keyboard shortcuts

| Action | Keys |
| --- | --- |
| Play / pause | Space |
| Next / previous song | Ctrl+→ / Ctrl+← |
| Seek 5 s forward / back | → / ← |
| Seek 30 s forward / back | Shift+→ / Shift+← |
| Volume up / down | Ctrl+↑ / Ctrl+↓ |
| Mute | M |
| Shuffle | Ctrl+S |
| Repeat (cycles) | Ctrl+R |
| Lyrics | Ctrl+L |
| Search (Library if open, else YouTube Music) | Ctrl+F |
| Queue / YouTube Music / Library | Ctrl+1 / 2 / 3 |
| Settings | Ctrl+, |
| Close lyrics, panel or dialog | Esc |
| Quit | Ctrl+Q |

The list is also in Settings → Keyboard shortcuts.

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
| Settings (equalizer and recent searches too) | `~/.config/ChaoticMusicPlayer/settings.conf` |
| Queue, playlists, library index, what was playing | `~/.local/share/ChaoticMusicPlayer/` |
| Found covers, lyrics and YouTube Music matches | `~/.cache/ChaoticMusicPlayer/` |

Everything stays on your computer. Your music files are never changed.

## Project layout

- `backend/`: plain C++ (no Qt): decoding, the equalizer and audio output,
  tags, library, playlists, lyrics, covers, YouTube Music. One folder per
  `.h` / `.cpp` pair.
- `wrapper/`: Qt objects and models that expose the backend to QML.
- `frontend/`: the QML interface.
- `scripts/`: launcher install scripts, and `shot-build.sh` / `shot-run.sh`,
  a screenshot test harness that runs the app offscreen in an isolated
  configuration (it refuses to run while the app itself is open).

## Note on YouTube Music

The YouTube Music features use unofficial interfaces and are meant for
personal use. They may stop working if YouTube changes them; updating yt-dlp
usually fixes playback problems.

## License

See [LICENSE](LICENSE).
