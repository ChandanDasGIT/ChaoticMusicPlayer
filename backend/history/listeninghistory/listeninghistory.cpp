#include "backend/history/listeninghistory/listeninghistory.h"

#include "backend/storage/tsv/tsv.h"

#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr const char *Header = "ChaoticMusicPlayer history v1";
constexpr size_t FieldCount = 8;

std::string line(const ListeningHistory::Play &play)
{
    const std::string fields[FieldCount] = {std::to_string(play.time), play.path,       play.title,
                                            play.artist,               play.album,      std::to_string(play.durationMs),
                                            play.thumbnailUrl,         play.from};
    std::string out;
    for (size_t i = 0; i < FieldCount; ++i) {
        if (i > 0)
            out += '\t';
        out += tsv::escape(fields[i]);
    }
    return out;
}

int64_t number(const std::string &s)
{
    try {
        return s.empty() ? 0 : std::stoll(s);
    } catch (...) {
        return 0;
    }
}

} // namespace

ListeningHistory::ListeningHistory(fs::path file)
    : m_file(std::move(file))
{
}

void ListeningHistory::load()
{
    m_plays.clear();
    std::ifstream in(m_file);
    std::string text;
    if (!in || !std::getline(in, text) || text != Header)
        return;
    while (std::getline(in, text)) {
        const std::vector<std::string> f = tsv::split(text);
        if (f.size() < FieldCount || f[1].empty())
            continue; // a damaged line (e.g. cut off by a crash) is skipped
        Play play;
        play.time = number(tsv::unescape(f[0]));
        play.path = tsv::unescape(f[1]);
        play.title = tsv::unescape(f[2]);
        play.artist = tsv::unescape(f[3]);
        play.album = tsv::unescape(f[4]);
        play.durationMs = number(tsv::unescape(f[5]));
        play.thumbnailUrl = tsv::unescape(f[6]);
        play.from = tsv::unescape(f[7]);
        m_plays.push_back(std::move(play));
    }
}

bool ListeningHistory::append(const Play &play)
{
    std::error_code ec;
    fs::create_directories(m_file.parent_path(), ec);
    const bool fresh = !fs::exists(m_file, ec) || fs::file_size(m_file, ec) == 0;
    std::ofstream out(m_file, std::ios::app);
    if (!out)
        return false;
    if (fresh)
        out << Header << '\n';
    out << line(play) << '\n';
    out.flush();
    if (!out)
        return false;
    m_plays.push_back(play);
    return true;
}

bool ListeningHistory::clear()
{
    std::error_code ec;
    fs::create_directories(m_file.parent_path(), ec);
    // Write to a temp file, then rename: a crash never leaves a half-written file.
    const fs::path temp = m_file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return false;
        out << Header << '\n';
        if (!out)
            return false;
    }
    fs::rename(temp, m_file, ec);
    if (ec)
        return false;
    m_plays.clear();
    return true;
}
