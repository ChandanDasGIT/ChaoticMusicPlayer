#include "backend/library/scanner/scanner.h"

#include "backend/library/metadata/metadata.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr size_t BatchSize = 100;
constexpr size_t ProgressEvery = 20;

constexpr std::array<std::string_view, 16> AudioExtensions = {
    ".mp3", ".flac", ".ogg", ".oga", ".opus", ".m4a", ".aac", ".wav",
    ".aiff", ".aif", ".wv", ".ape", ".wma", ".mpc", ".dsf", ".dff",
};

bool isHidden(const fs::path &path)
{
    const std::string name = path.filename().string();
    return !name.empty() && name.front() == '.';
}

} // namespace

Scanner::Scanner(Callbacks callbacks)
    : m_callbacks(std::move(callbacks))
    , m_thread(&Scanner::run, this)
{
}

Scanner::~Scanner()
{
    {
        std::lock_guard lock(m_mutex);
        m_quit = true;
    }
    m_wake.notify_one();
    m_thread.join();
}

void Scanner::scan(std::vector<std::string> paths)
{
    {
        std::lock_guard lock(m_mutex);
        m_jobs.push_back(std::move(paths));
    }
    m_wake.notify_one();
}

bool Scanner::isAudioFile(const fs::path &path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return std::find(AudioExtensions.begin(), AudioExtensions.end(), ext) != AudioExtensions.end();
}

void Scanner::run()
{
    for (;;) {
        std::vector<std::string> paths;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [this] { return m_quit || !m_jobs.empty(); });
            if (m_quit)
                return;
            paths = std::move(m_jobs.front());
            m_jobs.pop_front();
        }

        process(paths);
        if (quitRequested())
            return;
        if (m_callbacks.finished)
            m_callbacks.finished();
    }
}

void Scanner::process(const std::vector<std::string> &paths)
{
    const std::vector<fs::path> files = collect(paths);
    const size_t total = files.size();
    if (m_callbacks.progress)
        m_callbacks.progress(0, total);

    std::vector<Track> batch;
    for (size_t i = 0; i < total; ++i) {
        if (quitRequested())
            return;

        batch.push_back(Metadata::read(files[i]));

        if (batch.size() >= BatchSize && m_callbacks.tracksFound) {
            m_callbacks.tracksFound(std::move(batch));
            batch.clear();
        }
        if ((i + 1) % ProgressEvery == 0 && m_callbacks.progress)
            m_callbacks.progress(i + 1, total);
    }

    if (!batch.empty() && m_callbacks.tracksFound)
        m_callbacks.tracksFound(std::move(batch));
    if (m_callbacks.progress)
        m_callbacks.progress(total, total);
}

std::vector<fs::path> Scanner::collect(const std::vector<std::string> &paths) const
{
    std::vector<fs::path> files;

    for (const std::string &input : paths) {
        const fs::path root(input);
        std::error_code ec;

        if (fs::is_regular_file(root, ec)) {
            if (isAudioFile(root))
                files.push_back(root);
            continue;
        }
        if (!fs::is_directory(root, ec))
            continue;

        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            const fs::directory_entry &entry = *it;
            if (isHidden(entry.path())) {
                if (entry.is_directory(ec))
                    it.disable_recursion_pending();
                continue;
            }
            if (entry.is_regular_file(ec) && isAudioFile(entry.path()))
                files.push_back(entry.path());
        }
    }

    // Stable, predictable order; the playlist applies its own sort afterwards.
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

bool Scanner::quitRequested()
{
    std::lock_guard lock(m_mutex);
    return m_quit;
}
