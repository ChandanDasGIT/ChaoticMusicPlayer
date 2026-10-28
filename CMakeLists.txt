cmake_minimum_required(VERSION 3.21)

project(ChaoticMusicPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 DBus Network)
qt_standard_project_setup(REQUIRES 6.8)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswresample)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)
pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-uc icu-i18n)
find_package(CURL REQUIRED)
find_package(nlohmann_json 3 REQUIRED)

# ---------------------------------------------------------------------------
# Backend: pure C++ engine. Must never link Qt.
# ---------------------------------------------------------------------------
add_library(backend STATIC
    backend/audioengine/audiooutput/audiooutput.cpp
    backend/audioengine/audiooutput/audiooutput.h
    backend/audioengine/decoder/decoder.cpp
    backend/audioengine/decoder/decoder.h
    backend/audioengine/player/player.cpp
    backend/audioengine/player/player.h
    backend/audioengine/ringbuffer/ringbuffer.h
    backend/audioengine/spectrum/spectrum.cpp
    backend/audioengine/spectrum/spectrum.h
    backend/library/coverfinder/coverfinder.cpp
    backend/library/coverfinder/coverfinder.h
    backend/library/metadata/metadata.cpp
    backend/library/metadata/metadata.h
    backend/library/musiclibrary/musiclibrary.cpp
    backend/library/musiclibrary/musiclibrary.h
    backend/library/scanner/scanner.cpp
    backend/library/scanner/scanner.h
    backend/library/track/track.h
    backend/lyrics/lrcparser/lrcparser.cpp
    backend/lyrics/lrcparser/lrcparser.h
    backend/lyrics/lyricsprovider/lyricsprovider.cpp
    backend/lyrics/lyricsprovider/lyricsprovider.h
    backend/lyrics/transliterator/transliterator.cpp
    backend/lyrics/transliterator/transliterator.h
    backend/network/httpclient/httpclient.cpp
    backend/network/httpclient/httpclient.h
    backend/storage/tsv/tsv.h
    backend/system/process/process.cpp
    backend/system/process/process.h
    backend/youtube/linklibrary/linklibrary.cpp
    backend/youtube/linklibrary/linklibrary.h
    backend/youtube/streamresolver/streamresolver.cpp
    backend/youtube/streamresolver/streamresolver.h
    backend/youtube/ytdlp/ytdlp.cpp
    backend/youtube/ytdlp/ytdlp.h
    backend/youtube/ytmatcher/ytmatcher.cpp
    backend/youtube/ytmatcher/ytmatcher.h
    backend/youtube/ytmetadatastore/ytmetadatastore.cpp
    backend/youtube/ytmetadatastore/ytmetadatastore.h
    backend/youtube/ytmusicclient/ytmusicclient.cpp
    backend/youtube/ytmusicclient/ytmusicclient.h
    backend/playlist/playlist/playlist.cpp
    backend/playlist/playlist/playlist.h
    backend/playlist/playlistlibrary/playlistlibrary.cpp
    backend/playlist/playlistlibrary/playlistlibrary.h
    backend/playlist/playliststore/playliststore.cpp
    backend/playlist/playliststore/playliststore.h
    backend/settings/settings/settings.cpp
    backend/settings/settings/settings.h
)

set_target_properties(backend PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(backend PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_options(backend PRIVATE -Wall -Wextra)
target_link_libraries(backend PRIVATE
    PkgConfig::FFMPEG
    PkgConfig::PIPEWIRE
    PkgConfig::TAGLIB
    PkgConfig::ICU
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# ---------------------------------------------------------------------------
# ChaoticStyle: the app's Qt Quick Controls style. It only replaces ToolTip;
# everything else falls back to the Basic style.
# ---------------------------------------------------------------------------
qt_add_library(chaoticstyle STATIC)
qt_add_qml_module(chaoticstyle
    URI ChaoticStyle
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/ChaoticStyle
    QML_FILES
        frontend/style/ToolTip.qml
    IMPORTS
        QtQuick.Controls.Basic
)

# ---------------------------------------------------------------------------
# Application: wrapper + frontend QML module
# ---------------------------------------------------------------------------
qt_add_executable(ChaoticMusicPlayer
    main.cpp
    # Not QML types, so not part of the QML module below
    wrapper/mpris/mprisadaptor.cpp
    wrapper/mpris/mprisadaptor.h
    wrapper/singleinstance/singleinstance.cpp
    wrapper/singleinstance/singleinstance.h
)

set_source_files_properties(frontend/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(ChaoticMusicPlayer
    URI ChaoticMusicPlayer
    VERSION 1.0
    # Keep the module's build output out of the way of the executable,
    # which has the same name.
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/ChaoticMusicPlayer
    SOURCES
        wrapper/albumart/albumartprovider.cpp
        wrapper/albumart/albumartprovider.h
        wrapper/albumart/covercontroller.cpp
        wrapper/albumart/covercontroller.h
        wrapper/albumart/coverpalette.cpp
        wrapper/albumart/coverpalette.h
        wrapper/library/librarycontroller.cpp
        wrapper/library/librarycontroller.h
        wrapper/locallibrary/librarysongsmodel.cpp
        wrapper/locallibrary/librarysongsmodel.h
        wrapper/locallibrary/locallibrarycontroller.cpp
        wrapper/locallibrary/locallibrarycontroller.h
        wrapper/lyrics/lyricscontroller.cpp
        wrapper/lyrics/lyricscontroller.h
        wrapper/player/playercontroller.cpp
        wrapper/player/playercontroller.h
        wrapper/playlist/playlistcontroller.cpp
        wrapper/playlist/playlistcontroller.h
        wrapper/playlist/playlistmodel.cpp
        wrapper/playlist/playlistmodel.h
        wrapper/playlists/playlistscontroller.cpp
        wrapper/playlists/playlistscontroller.h
        wrapper/settings/settingscontroller.cpp
        wrapper/settings/settingscontroller.h
        wrapper/visualizer/visualizercontroller.cpp
        wrapper/visualizer/visualizercontroller.h
        wrapper/youtube/youtubecontroller.cpp
        wrapper/youtube/youtubecontroller.h
        wrapper/youtube/ytmetadatacontroller.cpp
        wrapper/youtube/ytmetadatacontroller.h
    QML_FILES
        Main.qml
        frontend/Theme.qml
        frontend/Icon.qml
        frontend/controls/ControlButton.qml
        frontend/controls/FadeLabel.qml
        frontend/controls/ListPopulate.qml
        frontend/controls/PillButton.qml
        frontend/controls/PlayingBars.qml
        frontend/controls/SeekBar.qml
        frontend/controls/ThemedMenu.qml
        frontend/controls/Toast.qml
        frontend/controls/ThemedSlider.qml
        frontend/controls/TransportControls.qml
        frontend/controls/VolumeSlider.qml
        frontend/detailsdialog/TrackDetailsDialog.qml
        frontend/dialogs/ConfirmDialog.qml
        frontend/dialogs/FolderPicker.qml
        frontend/dialogs/NameDialog.qml
        frontend/dropzone/DropOverlay.qml
        frontend/layout/AppShortcuts.qml
        frontend/layout/DockSlot.qml
        frontend/library/LibraryPanel.qml
        frontend/library/LibrarySongMenu.qml
        frontend/library/LibrarySongRow.qml
        frontend/lyrics/LyricsView.qml
        frontend/nowplaying/AlbumArt.qml
        frontend/nowplaying/MiniPlayer.qml
        frontend/nowplaying/NowPlayingPanel.qml
        frontend/nowplaying/QualityChip.qml
        frontend/nowplaying/Visualizer.qml
        frontend/playlist/PlaylistContextMenu.qml
        frontend/settingsdialog/SettingsDialog.qml
        frontend/settingsdialog/SettingsRow.qml
        frontend/settingsdialog/SettingsSwitch.qml
        frontend/youtube/YouTubePanel.qml
        frontend/youtube/YtHomeView.qml
        frontend/youtube/YtLibraryView.qml
        frontend/youtube/YtSavedLinksView.qml
        frontend/youtube/YtSearchView.qml
        frontend/youtube/YtSongMenu.qml
        frontend/youtube/YtStatus.qml
        frontend/youtube/YtThumbnail.qml
        frontend/youtube/YtTrackRow.qml
        frontend/playlist/PlaylistHeader.qml
        frontend/playlist/PlaylistPanel.qml
        frontend/playlist/PlaylistRow.qml
        frontend/playlists/AddToPlaylistMenu.qml
    RESOURCES
        frontend/fonts/fa-solid-900.ttf
        resources/icons/chaoticmusicplayer.svg
)

# Wrapper folders are include paths so the generated QML type registration
# can find the headers of QML_ELEMENT classes.
target_include_directories(ChaoticMusicPlayer PRIVATE
    ${CMAKE_SOURCE_DIR}
    wrapper/albumart
    wrapper/library
    wrapper/locallibrary
    wrapper/lyrics
    wrapper/player
    wrapper/playlist
    wrapper/playlists
    wrapper/settings
    wrapper/visualizer
    wrapper/youtube
)

target_compile_options(ChaoticMusicPlayer PRIVATE -Wall -Wextra)

target_link_libraries(ChaoticMusicPlayer PRIVATE
    backend
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::DBus
    Qt6::Network
    chaoticstyleplugin
)
