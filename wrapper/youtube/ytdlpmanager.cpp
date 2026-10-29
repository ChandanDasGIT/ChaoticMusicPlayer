#include "wrapper/youtube/ytdlpmanager.h"

#include "backend/network/httpclient/httpclient.h"
#include "backend/youtube/ytdlp/ytdlp.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThreadPool>

#ifndef CMP_YTDLP_SELF_UPDATE
#define CMP_YTDLP_SELF_UPDATE 1
#endif

namespace {

#ifdef Q_OS_WIN
const QString ProgramName = QStringLiteral("yt-dlp.exe");
#else
const QString ProgramName = QStringLiteral("yt-dlp");
#endif

// The app's own copy (it may update this one)
QString managedPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/ChaoticMusicPlayer/yt-dlp/") + ProgramName;
}

// A copy shipped next to the app (Windows packages, AppImage)
QString bundledPath()
{
    return QCoreApplication::applicationDirPath() + QLatin1Char('/') + ProgramName;
}

// yt-dlp's standalone build for this platform ("" = none)
QString assetName()
{
    const QString cpu = QSysInfo::currentCpuArchitecture();
#if defined(Q_OS_WIN)
    if (cpu == QLatin1String("arm64"))
        return QStringLiteral("yt-dlp_arm64.exe");
    if (cpu == QLatin1String("i386"))
        return QStringLiteral("yt-dlp_x86.exe");
    return QStringLiteral("yt-dlp.exe");
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (cpu == QLatin1String("x86_64"))
        return QStringLiteral("yt-dlp_linux");
    if (cpu == QLatin1String("arm64"))
        return QStringLiteral("yt-dlp_linux_aarch64");
    return {};
#else
    return {};
#endif
}

struct Outcome
{
    QString program; // what to run ("" = nothing works)
    QString version;
    QString source;
    QString status;
};

// The newest copy that runs (any thread).
Outcome pickCopy()
{
    Outcome best;
    const QList<QPair<QString, QString>> candidates = {
        {managedPath(), QStringLiteral("app")},
        {bundledPath(), QStringLiteral("bundled")},
        {QStringLiteral("yt-dlp"), QStringLiteral("system")},
    };
    for (const auto &[program, source] : candidates) {
        if (source != QLatin1String("system") && !QFile::exists(program))
            continue;
        const auto version = YtDlp::versionOf(program.toStdString());
        if (!version)
            continue;
        if (best.program.isEmpty() || YtDlp::olderThan(best.version.toStdString(), *version)) {
            best.program = program;
            best.version = QString::fromStdString(*version);
            best.source = source;
        }
    }
    return best;
}

// Downloads `asset` of the latest release (tag `latest`), checks its SHA-256 and
// installs it as the app's copy. "" = done, else what went wrong.
QString install(const QJsonObject &release, const QString &asset)
{
    QString assetUrl, sumsUrl;
    for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject a = value.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        if (name == asset)
            assetUrl = a.value(QStringLiteral("browser_download_url")).toString();
        else if (name == QLatin1String("SHA2-256SUMS"))
            sumsUrl = a.value(QStringLiteral("browser_download_url")).toString();
    }
    if (assetUrl.isEmpty() || sumsUrl.isEmpty())
        return QObject::tr("this release has no build for this computer");

    const HttpResponse sums = HttpClient::get(sumsUrl.toStdString(), {}, 30000);
    if (!sums.ok())
        return QObject::tr("couldn't download it");
    QString expected;
    for (const QString &line : QString::fromStdString(sums.body).split(QLatin1Char('\n'))) {
        const QStringList parts = line.simplified().split(QLatin1Char(' '));
        if (parts.size() == 2 && parts[1] == asset)
            expected = parts[0].toLower();
    }
    const HttpResponse file = HttpClient::get(assetUrl.toStdString(), {}, 300000);
    if (!file.ok() || file.body.empty())
        return QObject::tr("couldn't download it");
    const QByteArray data(file.body.data(), qsizetype(file.body.size()));
    if (expected.isEmpty() || QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex() != expected.toLatin1())
        return QObject::tr("the download didn't match yt-dlp's checksum, so it wasn't used");

    // Written next to it first, then put in place
    const QString target = managedPath();
    QDir().mkpath(QFileInfo(target).absolutePath());
    const QString temp = target + QStringLiteral(".download");
    QFile out(temp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
        QFile::remove(temp);
        return QObject::tr("couldn't save it");
    }
    out.close();
    out.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup
                       | QFile::ReadOther | QFile::ExeOther);
    QFile::remove(target);
    if (!QFile::rename(temp, target)) {
        QFile::remove(temp);
        return QObject::tr("couldn't save it");
    }
    return {};
}

// Picks a copy and, if asked, updates it (any thread).
Outcome work(bool checkOnline)
{
    Outcome best = pickCopy();
    if (!checkOnline)
        return best;

    const HttpResponse response = HttpClient::get("https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest",
                                                  {"Accept: application/vnd.github+json"}, 15000);
    const QJsonObject release = QJsonDocument::fromJson(QByteArray::fromStdString(response.body)).object();
    const QString latest = release.value(QStringLiteral("tag_name")).toString();
    if (!response.ok() || latest.isEmpty()) {
        best.status = QObject::tr("Couldn't check for updates (offline?).");
        return best;
    }
    if (!best.program.isEmpty() && !YtDlp::olderThan(best.version.toStdString(), latest.toStdString())) {
        best.status = QObject::tr("Up to date.");
        return best;
    }
    const QString error = install(release, assetName());
    if (!error.isEmpty()) {
        best.status = QObject::tr("Couldn't update to %1: %2.").arg(latest, error);
        return best;
    }
    Outcome updated = pickCopy();
    updated.status = updated.version == latest ? QObject::tr("Updated to version %1.").arg(latest)
                                               : QObject::tr("Downloaded version %1, but it doesn't run here.").arg(latest);
    return updated;
}

} // namespace

YtDlpManager::YtDlpManager(QObject *parent)
    : QObject(parent)
{
}

YtDlpManager::~YtDlpManager()
{
    m_shuttingDown = true;
}

bool YtDlpManager::canUpdate()
{
    return CMP_YTDLP_SELF_UPDATE && !assetName().isEmpty();
}

void YtDlpManager::start()
{
    // Songs may start before the check below is done: until then, the app's
    // own copy (or the shipped one) if there is one.
    if (QFile::exists(managedPath()))
        YtDlp::setExecutable(managedPath().toStdString());
    else if (QFile::exists(bundledPath()))
        YtDlp::setExecutable(bundledPath().toStdString());
    run(canUpdate());
}

void YtDlpManager::checkForUpdate()
{
    if (canUpdate())
        run(true);
}

void YtDlpManager::run(bool checkOnline)
{
    if (m_busy)
        return;
    m_busy = true;
    if (checkOnline)
        m_status = tr("Checking for updates…");
    emit changed();

    QPointer<YtDlpManager> self(this);
    QThreadPool::globalInstance()->start([self, checkOnline] {
        const Outcome outcome = work(checkOnline);
        YtDlp::setExecutable(outcome.program.isEmpty() ? std::string("yt-dlp") : outcome.program.toStdString());
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, outcome] {
            if (!self || self->m_shuttingDown)
                return;
            self->m_busy = false;
            self->m_version = outcome.version;
            self->m_source = outcome.source;
            if (!outcome.status.isEmpty())
                self->m_status = outcome.status;
            emit self->changed();
        }, Qt::QueuedConnection);
    });
}
