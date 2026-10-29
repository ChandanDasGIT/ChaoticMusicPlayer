#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Files and folders for QML (`Files` singleton), the same on every platform:
// QML never builds paths itself ("file://" + "/"), it asks here. Paths use "/"
// between folders (see release/README.md); drive letters ("C:/") and Android
// storage work too.
class Files : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Files(QObject *parent = nullptr);

    Q_INVOKABLE QString toPath(const QUrl &url) const;
    Q_INVOKABLE QUrl toUrl(const QString &path) const;
    // For showing: the platform's own separators ("C:\Music")
    Q_INVOKABLE QString displayPath(const QUrl &url) const;
    Q_INVOKABLE QUrl child(const QUrl &folder, const QString &name) const;
    Q_INVOKABLE bool isRoot(const QUrl &folder) const;
    Q_INVOKABLE QUrl parentOf(const QUrl &folder) const; // a root: itself
    // The path bar: the root, then each folder down to this one: [{ name, url }]
    Q_INVOKABLE QVariantList crumbs(const QUrl &folder) const;
    // Places beyond home for the folder picker: [{ name, url }]. Linux: other
    // disks and USB sticks (/mnt, /media); Windows: every drive; Android: the
    // phone's storage and SD cards.
    Q_INVOKABLE QVariantList drives() const;
};
