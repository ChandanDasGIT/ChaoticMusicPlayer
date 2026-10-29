#pragma once

#include <QObject>
#include <QString>

// Which yt-dlp the app runs, and keeping it up to date. At startup it picks
// the newest copy that works: the app's own (in its data folder), one shipped
// next to the app (Windows packages, AppImage) or the one installed on the
// system (PATH). Then it asks GitHub for yt-dlp's latest release and, if that
// is newer, downloads the standalone build for this platform, checks it
// against yt-dlp's published SHA-256 sums and makes it the app's own copy.
// Offline, nothing changes. Builds made with CMP_YTDLP_SELF_UPDATE off (e.g.
// for Flathub, which updates yt-dlp itself) never download anything.
class YtDlpManager : public QObject
{
    Q_OBJECT

public:
    explicit YtDlpManager(QObject *parent);
    ~YtDlpManager() override;

    void start();          // pick a copy, then check for an update (in the background)
    void checkForUpdate(); // the Settings button

    bool available() const { return !m_version.isEmpty(); }
    QString version() const { return m_version; }
    QString source() const { return m_source; } // "app", "bundled", "system" or ""
    QString status() const { return m_status; } // the last check, in words ("" = none yet)
    bool busy() const { return m_busy; }
    static bool canUpdate(); // this build may download yt-dlp, for this platform

signals:
    void changed();

private:
    void run(bool checkOnline);

    QString m_version;
    QString m_source;
    QString m_status;
    bool m_busy = false;
    bool m_shuttingDown = false;
};
