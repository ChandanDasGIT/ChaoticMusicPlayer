#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

class SettingsController;

// Android only: asks GitHub at startup whether a newer Tuned Chaos is out and,
// if asked to, downloads its APK and hands it to Android's installer. A check
// that fails stays silent: being offline is normal. A download carries on from
// where it stopped, also after a restart. (The other systems update through
// their own packages.)
class AppUpdater : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool downloading READ downloading NOTIFY stateChanged)
    Q_PROPERTY(bool needsPermission READ needsPermission NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged) // why the download failed
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged) // percent

public:
    struct Release
    {
        QString version; // "" = no usable release
        QString apkUrl;
        qint64 apkSize = 0;
        QString apkSha256; // "" = GitHub didn't say
    };

    AppUpdater(SettingsController *settings, QObject *parent);
    ~AppUpdater() override;

    void check(); // in the background; off in Settings > About = no check

    bool downloading() const { return m_downloading; }
    bool needsPermission() const { return m_needsPermission; }
    QString error() const { return m_error; }
    int progress() const { return m_progress; }

    Q_INVOKABLE void startUpdate();
    Q_INVOKABLE void openInstallSettings();
    Q_INVOKABLE void skipThisVersion(); // until a newer one comes out
    Q_INVOKABLE void cancel();          // what was downloaded stays, for next time

signals:
    void updateAvailable(const QString &version);
    void installStarted();
    void stateChanged();
    void progressChanged();

private:
    void continueInstall();

    SettingsController *m_settings;
    Release m_release;
    QString m_apkPath; // downloaded, waiting to be installed
    QString m_error;
    int m_progress = 0;
    bool m_downloading = false;
    bool m_needsPermission = false;
    std::shared_ptr<std::atomic_bool> m_cancelled;
};
