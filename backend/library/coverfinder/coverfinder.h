#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct CoverQuery
{
    std::string path; // local audio file
    std::string title;
    std::string artist;
    std::string album;
    std::string albumArtist; // "" = not tagged
    int64_t durationMs = 0;
    std::string knownYtThumbnail; // from the background YouTube Music lookup, if it matched
    std::string knownYtAlbum;     // that match's album (its thumbnail is used only if it is ours)
};

// Album art for songs without an embedded cover. Asks every enabled source
// and keeps the largest image:
//   "folder"      - cover.jpg / folder.png / front.jpg ... next to the song
//   "ytmusic"     - YouTube Music (the song's match)
//   "itunes"      - Apple's iTunes search
//   "musicbrainz" - Cover Art Archive (MusicBrainz release groups)
// An online result counts only if its album is ours (equal once edition words
// like "Deluxe" or "Original Soundtrack" are dropped) and its artist is the
// album artist or one of the song's artists.
// Online results are cached per album (per song when it has no album) in
// `cacheDir`; "nothing found" is remembered for a week. The user's files are
// never changed. Blocking: call from worker threads. Thread-safe.
class CoverFinder
{
public:
    struct Image
    {
        std::vector<unsigned char> data;
        int width = 0;
        int height = 0;
        std::string source;
    };

    explicit CoverFinder(std::filesystem::path cacheDir);

    // What has already been found for this song (cache or folder image), without
    // going online. Empty data = none known.
    Image cached(const CoverQuery &query) const;
    // Online lookup was done and found nothing (recently).
    bool knownMissing(const CoverQuery &query) const;
    // find() already ran for this album (found something, or recently nothing).
    bool lookedUp(const CoverQuery &query) const;

    // Looks everywhere and caches the best result. Empty data = nothing found.
    // improved() is called (on this thread) each time a better image arrives,
    // already cached, so it can be shown before the slower sources answer.
    Image find(const CoverQuery &query, const std::vector<std::string> &sources,
               const std::function<void(const Image &)> &improved = {}) const;

    // Deletes the cache of older versions (their matching was too loose).
    void removeOldCache() const;

    // Same album, ignoring case, punctuation and edition words.
    static bool sameAlbum(const std::string &ours, const std::string &theirs);

    static Image folderImage(const std::string &audioPath);
    // Width / height of a JPEG or PNG without decoding it (0 if unknown).
    static bool imageSize(const std::vector<unsigned char> &data, int &width, int &height);

private:
    std::filesystem::path cacheFile(const CoverQuery &query, const char *suffix) const;

    std::filesystem::path m_cacheDir;
};
