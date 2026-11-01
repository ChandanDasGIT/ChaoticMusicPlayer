#include "wrapper/update/appupdater.h"

#include "backend/network/httpclient/httpclient.h"
#include "wrapper/settings/settingscontroller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJniObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QThreadPool>
#include <QVersionNumber>

#include <functional>

namespace {

using Release = AppUpdater::Release;

constexpr char kInstaller[] = "io/github/chandandasgit/tunedchaos/AppInstaller";

QJniObject context()
{
    return QNativeInterface::QAndroidApplication::context();
}

QString apkFolder()
{
    return QJniObject::callStaticObjectMethod(kInstaller, "apkFolder", "(Landroid/content/Context;)Ljava/lang/String;",
                                              context().object()).toString();
}

// v1.2 is 1.2. Anything else in a tag is not a version.
QString versionOfTag(const QString &tag)
{
    const QString text = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
    qsizetype end = 0;
    const QVersionNumber version = QVersionNumber::fromString(text, &end);
    return version.isNull() || end != text.size() ? QString() : text;
}

// (normalized: without it 1.1.0 would count as newer than 1.1)
bool isNewer(const QString &version, const QString &running)
{
    return QVersionNumber::fromString(version).normalized() > QVersionNumber::fromString(running).normalized();
}

// Blocks: worker threads only.
Release latestRelease()
{
    const HttpResponse response = HttpClient::get("https://api.github.com/repos/ChandanDasGIT/TunedChaos/releases/latest",
                                                  {"Accept: application/vnd.github+json"}, 15000);
    if (!response.ok())
        return {};
    const QJsonObject release = QJsonDocument::fromJson(QByteArray::fromStdString(response.body)).object();
    const QString version = versionOfTag(release.value(QStringLiteral("tag_name")).toString());
    if (version.isEmpty())
        return {};
    for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
        const qint64 size = asset.value(QStringLiteral("size")).toInteger();
        const QString digest = asset.value(QStringLiteral("digest")).toString(); // "sha256:..."
        if (asset.value(QStringLiteral("name")).toString().endsWith(QStringLiteral(".apk"), Qt::CaseInsensitive)
            && !url.isEmpty() && size > 0)
            return {version, url, size, digest.startsWith(QStringLiteral("sha256:")) ? digest.mid(7) : QString()};
    }
    return {}; // a release without an APK has nothing to offer here
}

bool isComplete(const QString &path, const Release &release)
{
    QFile file(path);
    if (file.size() != release.apkSize || !file.open(QIODevice::ReadOnly))
        return false;
    if (release.apkSha256.isEmpty())
        return true;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && hash.result().toHex() == release.apkSha256.toLatin1();
}

// Blocks: worker threads only. Returns what went wrong ("" = the APK is ready, or stopped by cancelled).
QString saveApk(const Release &release, const QString &apk, const std::atomic_bool &cancelled,
                const std::function<void(int)> &progress)
{
    const QFileInfo target(apk);
    const QString part = target.path() + QLatin1Char('/') + release.version + QStringLiteral(".part");
    if (!QDir().mkpath(target.path()))
        return QObject::tr("Couldn't save the download.");
    for (const QFileInfo &file : QDir(target.path()).entryInfoList(QDir::Files)) // other versions'
        if (file.filePath() != apk && file.filePath() != part)
            QFile::remove(file.filePath());

    if (!target.exists()) {
        if (QFileInfo(part).size() > release.apkSize)
            QFile::remove(part);
        if (QFileInfo(part).size() < release.apkSize) {
            int shown = -1;
            const HttpResponse response = HttpClient::download(release.apkUrl.toStdString(), part.toStdString(), [&](long long size) {
                const int percent = int(size * 100 / release.apkSize);
                if (percent != shown)
                    progress(shown = percent);
                return !cancelled.load();
            });
            if (cancelled.load())
                return {};
            if (!response.ok())
                return QObject::tr("The download failed. Try again.");
        }
        if (!QFile::rename(part, apk))
            return QObject::tr("Couldn't save the download.");
    }
    // Also a copy from an earlier start: Android must never be handed a damaged one, again and again
    if (!isComplete(apk, release)) {
        QFile::remove(apk);
        return QObject::tr("The download was damaged. Try again.");
    }
    return {};
}

} // namespace

AppUpdater::AppUpdater(SettingsController *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    // Back from Android's settings page, or from the background (Android lets
    // only the app on screen open the installer).
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            continueInstall();
    });
}

AppUpdater::~AppUpdater()
{
    if (m_cancelled)
        m_cancelled->store(true);
}

void AppUpdater::check()
{
    // What an earlier update left: that version, running now, or an older one
    const QString folder = apkFolder();
    if (!folder.isEmpty())
        for (const QFileInfo &file : QDir(folder).entryInfoList(QDir::Files))
            if (!isNewer(file.completeBaseName(), QStringLiteral(APP_VERSION)))
                QFile::remove(file.filePath());

    if (!m_settings->appUpdateCheck())
        return;

    QPointer<AppUpdater> self(this);
    QThreadPool::globalInstance()->start([self] {
        const Release release = latestRelease();
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, release] {
            if (!self || release.version.isEmpty() || !isNewer(release.version, QStringLiteral(APP_VERSION))
                || release.version == self->m_settings->appUpdateSkipped())
                return;
            self->m_release = release;
            emit self->updateAvailable(release.version);
        }, Qt::QueuedConnection);
    });
}

void AppUpdater::startUpdate()
{
    if (m_downloading || m_release.version.isEmpty())
        return;
    const QString folder = apkFolder();
    if (folder.isEmpty()) {
        m_error = tr("Couldn't save the download.");
        emit stateChanged();
        return;
    }
    const QString apk = folder + QLatin1Char('/') + m_release.version + QStringLiteral(".apk");
    m_error.clear();
    m_progress = 0;
    m_downloading = true;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    emit progressChanged();
    emit stateChanged();

    QPointer<AppUpdater> self(this);
    QThreadPool::globalInstance()->start([self, release = m_release, apk, cancelled = m_cancelled] {
        const QString error = saveApk(release, apk, *cancelled, [self](int percent) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [self, percent] {
                if (!self)
                    return;
                self->m_progress = percent;
                emit self->progressChanged();
            }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, apk, error, cancelled] {
            if (!self)
                return;
            self->m_downloading = false;
            if (!cancelled->load()) {
                self->m_error = error;
                if (error.isEmpty())
                    self->m_apkPath = apk;
            }
            emit self->stateChanged();
            self->continueInstall();
        }, Qt::QueuedConnection);
    });
}

void AppUpdater::continueInstall()
{
    if (m_apkPath.isEmpty() || QGuiApplication::applicationState() != Qt::ApplicationActive)
        return;
    const bool allowed = QJniObject::callStaticMethod<jboolean>(kInstaller, "canInstall", "(Landroid/content/Context;)Z",
                                                                context().object());
    m_needsPermission = !allowed;
    if (!allowed) {
        emit stateChanged();
        return;
    }

    const QJniObject path = QJniObject::fromString(m_apkPath);
    m_apkPath.clear();
    const bool started = QJniObject::callStaticMethod<jboolean>(kInstaller, "install",
                                                                "(Landroid/content/Context;Ljava/lang/String;)Z",
                                                                context().object(), path.object<jstring>());
    if (started) {
        emit installStarted();
        return;
    }
    m_error = tr("Couldn't open Android's installer.");
    emit stateChanged();
}

void AppUpdater::openInstallSettings()
{
    if (!QJniObject::callStaticMethod<jboolean>(kInstaller, "openInstallSettings", "(Landroid/content/Context;)Z",
                                                context().object())) {
        m_error = tr("Couldn't open Android's settings.");
        emit stateChanged();
    }
}

void AppUpdater::skipThisVersion()
{
    m_settings->setAppUpdateSkipped(m_release.version);
}

void AppUpdater::cancel()
{
    if (m_cancelled)
        m_cancelled->store(true);
    m_apkPath.clear();
    m_needsPermission = false;
    m_error.clear();
    emit stateChanged();
}
