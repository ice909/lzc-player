#include "src/app/playeripcserver.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>

#include "src/player/videoplayerview.h"

namespace
{

Q_LOGGING_CATEGORY(lzcPlayerIpcLog, "lzc-player.ipc")

QString socketNameForPlatform(QString serverName)
{
#ifdef Q_OS_WIN
    if (!serverName.startsWith(QStringLiteral("\\\\.\\pipe\\")))
    {
        serverName.prepend(QStringLiteral("\\\\.\\pipe\\"));
    }
#endif
    return serverName;
}

}

PlayerIpcServer::PlayerIpcServer(const QString &serverName, QObject *parent)
    : QObject(parent),
      m_serverName(socketNameForPlatform(serverName.trimmed())),
      m_server(new QLocalServer(this)),
      m_view(nullptr)
{
    connect(m_server, &QLocalServer::newConnection, this, &PlayerIpcServer::handleNewConnection);
}

PlayerIpcServer::~PlayerIpcServer()
{
    for (QLocalSocket *socket : std::as_const(m_sockets))
    {
        if (!socket)
        {
            continue;
        }

        socket->disconnect(this);
        socket->disconnectFromServer();
        socket->deleteLater();
    }

    if (m_server->isListening())
    {
        m_server->close();
    }
}

bool PlayerIpcServer::start()
{
    if (m_serverName.isEmpty())
    {
        return false;
    }

#ifndef Q_OS_WIN
    QLocalServer::removeServer(m_serverName);
#endif

    if (!m_server->listen(m_serverName))
    {
        qCritical(lzcPlayerIpcLog) << "Failed to listen on player IPC server:" << m_serverName
                                   << "error:" << m_server->errorString();
        return false;
    }

    qInfo(lzcPlayerIpcLog) << "Listening on player IPC server:" << m_server->fullServerName();
    return true;
}

void PlayerIpcServer::setVideoPlayer(VideoPlayerView *view)
{
    m_view = view;
}

void PlayerIpcServer::broadcastEvent(const QString &name, const QVariant &payload)
{
    QVariantMap message{
        {QStringLiteral("type"), QStringLiteral("event")},
        {QStringLiteral("name"), name},
        {QStringLiteral("payload"), payload},
    };
    broadcastMessage(message);
}

void PlayerIpcServer::emitInitialState()
{
    const auto sockets = m_sockets;
    for (QLocalSocket *socket : sockets)
    {
        emitInitialStateToSocket(socket);
    }
}

void PlayerIpcServer::emitInitialStateToSocket(QLocalSocket *socket)
{
    if (!m_view)
    {
        return;
    }

    sendToSocket(socket, QVariantMap{
        {QStringLiteral("type"), QStringLiteral("event")},
        {QStringLiteral("name"), QStringLiteral("progress")},
        {QStringLiteral("payload"), QVariantMap{
        {QStringLiteral("timePos"), m_view->timePos()},
        {QStringLiteral("duration"), m_view->duration()},
        {QStringLiteral("playing"), m_view->isPlaying()},
        }},
    });
    sendToSocket(socket, QVariantMap{{QStringLiteral("type"), QStringLiteral("event")}, {QStringLiteral("name"), QStringLiteral("playing")}, {QStringLiteral("payload"), m_view->isPlaying()}});
    sendToSocket(socket, QVariantMap{{QStringLiteral("type"), QStringLiteral("event")}, {QStringLiteral("name"), QStringLiteral("playbackSpeed")}, {QStringLiteral("payload"), m_view->playbackSpeed()}});
    sendToSocket(socket, QVariantMap{{QStringLiteral("type"), QStringLiteral("event")}, {QStringLiteral("name"), QStringLiteral("volume")}, {QStringLiteral("payload"), m_view->volume()}});
    sendToSocket(socket, QVariantMap{{QStringLiteral("type"), QStringLiteral("event")}, {QStringLiteral("name"), QStringLiteral("subtitleId")}, {QStringLiteral("payload"), m_view->subtitleId()}});
    sendToSocket(socket, QVariantMap{{QStringLiteral("type"), QStringLiteral("event")}, {QStringLiteral("name"), QStringLiteral("subtitleTracks")}, {QStringLiteral("payload"), m_view->subtitleTracks()}});
    sendToSocket(socket, QVariantMap{{QStringLiteral("type"), QStringLiteral("event")}, {QStringLiteral("name"), QStringLiteral("qualityLabel")}, {QStringLiteral("payload"), m_view->qualityLabel()}});
}

void PlayerIpcServer::emitProcessReady()
{
    const auto sockets = m_sockets;
    for (QLocalSocket *socket : sockets)
    {
        emitProcessReadyToSocket(socket);
    }
}

void PlayerIpcServer::emitProcessReadyToSocket(QLocalSocket *socket)
{
    sendToSocket(socket, QVariantMap{
        {QStringLiteral("type"), QStringLiteral("event")},
        {QStringLiteral("name"), QStringLiteral("process-ready")},
        {QStringLiteral("payload"), QVariantMap{
        {QStringLiteral("sourceUrl"), m_view ? m_view->currentSourceUrl() : QString()},
        {QStringLiteral("currentEpisode"), currentEpisodePayload()},
        }},
    });
}

void PlayerIpcServer::handleNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QLocalSocket *socket = m_server->nextPendingConnection();
        if (!socket)
        {
            continue;
        }

        m_sockets.append(socket);
        connect(socket, &QLocalSocket::disconnected, this, &PlayerIpcServer::handleSocketDisconnected);
        emitInitialStateToSocket(socket);
        emitProcessReadyToSocket(socket);
    }
}

void PlayerIpcServer::handleSocketDisconnected()
{
    auto *socket = qobject_cast<QLocalSocket *>(sender());
    if (!socket)
    {
        return;
    }

    removeSocket(socket);
}

QVariantMap PlayerIpcServer::currentEpisodePayload() const
{
    if (!m_view)
    {
        return {};
    }

    return m_view->currentEpisodePayload();
}

void PlayerIpcServer::sendToSocket(QLocalSocket *socket, const QVariantMap &message)
{
    if (!socket || socket->state() != QLocalSocket::ConnectedState)
    {
        return;
    }

    const QByteArray encoded = QJsonDocument::fromVariant(message).toJson(QJsonDocument::Compact) + '\n';
    if (socket->write(encoded) == -1)
    {
        qWarning(lzcPlayerIpcLog) << "Failed to write player IPC event:" << socket->errorString();
        removeSocket(socket);
        return;
    }

    socket->flush();
}

void PlayerIpcServer::broadcastMessage(const QVariantMap &message)
{
    const auto sockets = m_sockets;
    for (QLocalSocket *socket : sockets)
    {
        sendToSocket(socket, message);
    }
}

void PlayerIpcServer::removeSocket(QLocalSocket *socket)
{
    m_sockets.removeAll(socket);
    if (!socket)
    {
        return;
    }

    socket->disconnect(this);
    socket->deleteLater();
}
