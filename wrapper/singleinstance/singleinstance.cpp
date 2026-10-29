#include "wrapper/singleinstance/singleinstance.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

namespace {

constexpr int ConnectTimeoutMs = 500;

} // namespace

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

SingleInstance::~SingleInstance()
{
    if (m_server)
        m_server->close(); // removes the socket file
}

void SingleInstance::stop()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

bool SingleInstance::start()
{
    m_socketPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
                   + QStringLiteral("/ChaoticMusicPlayer.sock");

    // Someone already listening? Ask them to come forward and step aside.
    QLocalSocket socket;
    socket.connectToServer(m_socketPath);
    if (socket.waitForConnected(ConnectTimeoutMs)) {
        socket.write("activate\n");
        socket.waitForBytesWritten(ConnectTimeoutMs);
        socket.disconnectFromServer();
        return false;
    }

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_socketPath)) {
        // Nobody answered above, so the socket file is left over from a crash.
        QLocalServer::removeServer(m_socketPath);
        if (!m_server->listen(m_socketPath)) {
            qWarning("Single instance: cannot listen on %s: %s", qPrintable(m_socketPath),
                     qPrintable(m_server->errorString()));
            return true; // run anyway, just without single-instance protection
        }
    }

    // Any connection is a second launch asking us to come forward.
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server->nextPendingConnection()) {
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
            emit activationRequested();
        }
    });
    return true;
}
