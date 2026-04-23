#ifndef LZC_PLAYER_PLAYERIPCSERVER_H_
#define LZC_PLAYER_PLAYERIPCSERVER_H_

#include <QObject>
#include <QVariantMap>

class QLocalServer;
class QLocalSocket;
class VideoPlayerView;

class PlayerIpcServer : public QObject
{
    Q_OBJECT

public:
    explicit PlayerIpcServer(const QString &serverName, QObject *parent = nullptr);
    ~PlayerIpcServer() override;

    bool start();
    void setVideoPlayer(VideoPlayerView *view);

public slots:
    void broadcastEvent(const QString &name, const QVariant &payload);
    void emitInitialState();
    void emitProcessReady();

private slots:
    void handleNewConnection();
    void handleSocketDisconnected();

private:
    QVariantMap currentEpisodePayload() const;
    void sendToSocket(QLocalSocket *socket, const QVariantMap &message);
    void broadcastMessage(const QVariantMap &message);
    void emitInitialStateToSocket(QLocalSocket *socket);
    void emitProcessReadyToSocket(QLocalSocket *socket);
    void removeSocket(QLocalSocket *socket);

    QString m_serverName;
    QLocalServer *m_server;
    QList<QLocalSocket *> m_sockets;
    VideoPlayerView *m_view;
};

#endif
