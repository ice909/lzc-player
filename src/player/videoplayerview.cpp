#include "src/player/videoplayerview.h"

#include <QMetaObject>
#include <QQuickWindow>

#include "src/app/playerwindow.h"
#include "src/player/mpvplayersession.h"

VideoPlayerView::VideoPlayerView(QQuickItem *parent)
    : QQuickItem(parent),
      m_session(new MpvPlayerSession(this)),
      m_registeredWindow(nullptr),
      m_renderContextReady(false),
      m_windowUpdateScheduled(false),
      m_playlistIndex(-1)
{
    setFlag(QQuickItem::ItemHasContents, false);

    connect(this, &QQuickItem::windowChanged, this, &VideoPlayerView::handleWindowChanged);
    connect(m_session, &MpvPlayerSession::playbackFinished, this, &VideoPlayerView::handlePlaybackFinished);
    connect(m_session, &MpvPlayerSession::playingChanged, this, &VideoPlayerView::playingChanged);
    connect(m_session, &MpvPlayerSession::timePosChanged, this, &VideoPlayerView::timePosChanged);
    connect(m_session, &MpvPlayerSession::durationChanged, this, &VideoPlayerView::durationChanged);
    connect(m_session, &MpvPlayerSession::bufferDurationChanged, this, &VideoPlayerView::bufferDurationChanged);
    connect(m_session, &MpvPlayerSession::bufferEndChanged, this, &VideoPlayerView::bufferEndChanged);
    connect(m_session, &MpvPlayerSession::loadingChanged, this, &VideoPlayerView::loadingChanged);
    connect(m_session, &MpvPlayerSession::bufferingChanged, this, &VideoPlayerView::bufferingChanged);
    connect(m_session, &MpvPlayerSession::seekingChanged, this, &VideoPlayerView::seekingChanged);
    connect(m_session, &MpvPlayerSession::hasMediaChanged, this, &VideoPlayerView::hasMediaChanged);
    connect(m_session, &MpvPlayerSession::bufferingProgressChanged, this, &VideoPlayerView::bufferingProgressChanged);
    connect(m_session, &MpvPlayerSession::playbackSpeedChanged, this, &VideoPlayerView::playbackSpeedChanged);
    connect(m_session, &MpvPlayerSession::volumeChanged, this, &VideoPlayerView::volumeChanged);
    connect(m_session, &MpvPlayerSession::qualityLabelChanged, this, &VideoPlayerView::qualityLabelChanged);
    connect(m_session, &MpvPlayerSession::videoTracksChanged, this, &VideoPlayerView::videoTracksChanged);
    connect(m_session, &MpvPlayerSession::videoIdChanged, this, &VideoPlayerView::videoIdChanged);
    connect(m_session, &MpvPlayerSession::subtitleTracksChanged, this, &VideoPlayerView::subtitleTracksChanged);
    connect(m_session, &MpvPlayerSession::subtitleIdChanged, this, &VideoPlayerView::subtitleIdChanged);
    connect(m_session, &MpvPlayerSession::consoleOpenChanged, this, &VideoPlayerView::consoleOpenChanged);
}

VideoPlayerView::~VideoPlayerView()
{
    detachFromWindow();
}

bool VideoPlayerView::isPlaying() const
{
    return m_session->isPlaying();
}

double VideoPlayerView::timePos() const
{
    return m_session->timePos();
}

double VideoPlayerView::duration() const
{
    return m_session->duration();
}

double VideoPlayerView::bufferDuration() const
{
    return m_session->bufferDuration();
}

double VideoPlayerView::bufferEnd() const
{
    return m_session->bufferEnd();
}

bool VideoPlayerView::loading() const
{
    return m_session->loading();
}

bool VideoPlayerView::buffering() const
{
    return m_session->buffering();
}

bool VideoPlayerView::seeking() const
{
    return m_session->seeking();
}

bool VideoPlayerView::hasMedia() const
{
    return m_session->hasMedia();
}

double VideoPlayerView::bufferingProgress() const
{
    return m_session->bufferingProgress();
}

double VideoPlayerView::playbackSpeed() const
{
    return m_session->playbackSpeed();
}

double VideoPlayerView::volume() const
{
    return m_session->volume();
}

QString VideoPlayerView::qualityLabel() const
{
    return m_session->qualityLabel();
}

QVariantList VideoPlayerView::videoTracks() const
{
    return m_session->videoTracks();
}

int VideoPlayerView::videoId() const
{
    return m_session->videoId();
}

QVariantList VideoPlayerView::subtitleTracks() const
{
    return m_session->subtitleTracks();
}

int VideoPlayerView::subtitleId() const
{
    return m_session->subtitleId();
}

bool VideoPlayerView::consoleOpen() const
{
    return m_session->consoleOpen();
}

QVariantList VideoPlayerView::playlistItems() const
{
    return m_playlistItems;
}

int VideoPlayerView::playlistIndex() const
{
    return m_playlistIndex;
}

int VideoPlayerView::playlistCount() const
{
    return m_playlistItems.size();
}

bool VideoPlayerView::hasPlaylist() const
{
    return !m_playlistItems.isEmpty();
}

QString VideoPlayerView::currentSourceUrl() const
{
    return m_currentSourceUrl;
}

QVariantMap VideoPlayerView::currentEpisodePayload() const
{
    return episodePayloadForItem(playlistItemAt(m_playlistIndex), m_playlistIndex);
}

void VideoPlayerView::loadFile(const QString &path)
{
    if (path.isEmpty())
    {
        return;
    }

    loadMedia(path, QVariantList{});
}

void VideoPlayerView::loadMedia(const QString &path, const QVariantList &externalSubtitles)
{
    m_currentSourceUrl = path;
    if (!m_renderContextReady)
    {
        m_pendingFile = path;
        m_pendingExternalSubtitles = externalSubtitles;
        scheduleWindowUpdate();
        return;
    }

    m_pendingFile.clear();
    m_pendingExternalSubtitles.clear();
    m_session->setExternalSubtitles(externalSubtitles);
    m_session->loadFile(path);
}

void VideoPlayerView::setPlaylistItems(const QVariantList &items)
{
    if (m_playlistItems == items)
    {
        return;
    }

    const int previousCount = m_playlistItems.size();
    m_playlistItems = items;
    m_playlistProgressByKey.clear();
    emit playlistItemsChanged();

    if (previousCount != m_playlistItems.size())
    {
        emit playlistCountChanged();
    }

    const int nextIndex = m_playlistItems.isEmpty() ? -1 : qBound(0, m_playlistIndex, m_playlistItems.size() - 1);
    setPlaylistIndexInternal(nextIndex);
}

void VideoPlayerView::playEpisode(int index)
{
    loadEpisodeAtIndex(index, QStringLiteral("user"));
}

void VideoPlayerView::playNextEpisode()
{
    if (m_playlistItems.isEmpty())
    {
        return;
    }

    loadEpisodeAtIndex(m_playlistIndex + 1, QStringLiteral("user"));
}

void VideoPlayerView::playPrevEpisode()
{
    if (m_playlistItems.isEmpty())
    {
        return;
    }

    loadEpisodeAtIndex(m_playlistIndex - 1, QStringLiteral("user"));
}

void VideoPlayerView::setStartupPosition(const QString &position)
{
    m_session->setStartupPosition(position);
}

void VideoPlayerView::togglePause()
{
    m_session->togglePause();
}

void VideoPlayerView::seekRelative(double seconds)
{
    m_session->seekRelative(seconds);
}

void VideoPlayerView::seekTo(double seconds)
{
    m_session->seekTo(seconds);
}

void VideoPlayerView::setPlaybackSpeed(double speed)
{
    m_session->setPlaybackSpeed(speed);
}

void VideoPlayerView::setVolume(double volume)
{
    m_session->setVolume(volume);
}

void VideoPlayerView::setVideoId(int id)
{
    m_session->setVideoId(id);
}

void VideoPlayerView::setSubtitleId(int id)
{
    m_session->setSubtitleId(id);
}

void VideoPlayerView::command(const QVariant &params)
{
    m_session->command(params);
}

QVariant VideoPlayerView::getProperty(const QString &name)
{
    return m_session->getProperty(name);
}

void VideoPlayerView::setProperty(const QString &name, const QVariant &value)
{
    m_session->setProperty(name, value);
}

void VideoPlayerView::handleWindowChanged(QQuickWindow *window)
{
    if (window == m_registeredWindow)
    {
        return;
    }

    detachFromWindow();
    attachToWindow(qobject_cast<PlayerWindow *>(window));
}

void VideoPlayerView::markRenderContextReady()
{
    if (m_renderContextReady)
    {
        return;
    }

    m_renderContextReady = true;
    loadPendingFile();
}

void VideoPlayerView::loadPendingFile()
{
    if (!m_renderContextReady || m_pendingFile.isEmpty())
    {
        return;
    }

    const QString pendingFile = m_pendingFile;
    m_pendingFile.clear();
    m_currentSourceUrl = pendingFile;
    const QVariantList pendingExternalSubtitles = m_pendingExternalSubtitles;
    m_pendingExternalSubtitles.clear();
    m_session->setExternalSubtitles(pendingExternalSubtitles);
    m_session->loadFile(pendingFile);
}

void VideoPlayerView::handlePlaybackFinished()
{
    if (m_playlistItems.isEmpty())
    {
        return;
    }

    loadEpisodeAtIndex(m_playlistIndex + 1, QStringLiteral("auto"));
}

QVariantMap VideoPlayerView::playlistItemAt(int index) const
{
    if (index < 0 || index >= m_playlistItems.size())
    {
        return {};
    }

    return m_playlistItems.at(index).toMap();
}

QString VideoPlayerView::normalizedPlaylistIdentity(const QVariantMap &item) const
{
    const QString id = item.value(QStringLiteral("id")).toString().trimmed();
    if (!id.isEmpty())
    {
        return id;
    }

    return item.value(QStringLiteral("url")).toString().trimmed();
}

QString VideoPlayerView::playlistProgressKey(const QVariantMap &item, int index) const
{
    const QString identity = normalizedPlaylistIdentity(item);
    if (!identity.isEmpty())
    {
        return identity;
    }

    return QStringLiteral("#index:%1").arg(index);
}

void VideoPlayerView::persistCurrentEpisodeProgress()
{
    if (m_playlistIndex < 0 || m_playlistIndex >= m_playlistItems.size())
    {
        return;
    }

    const QVariantMap item = playlistItemAt(m_playlistIndex);
    const QString key = playlistProgressKey(item, m_playlistIndex);
    if (key.isEmpty())
    {
        return;
    }

    PlaylistProgressEntry entry;
    entry.positionSeconds = timePos();
    entry.durationSeconds = duration();
    m_playlistProgressByKey.insert(key, entry);
}

QString VideoPlayerView::currentEpisodeDisplayName(const QVariantMap &item, int index) const
{
    const QString name = item.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty())
    {
        return name;
    }

    if (index >= 0)
    {
        return QStringLiteral("第 %1 集").arg(index + 1);
    }

    return {};
}

QVariantMap VideoPlayerView::episodePayloadForItem(const QVariantMap &item, int index) const
{
    if (item.isEmpty())
    {
        return {};
    }

    QVariantMap payload{
        {QStringLiteral("index"), index},
        {QStringLiteral("url"), item.value(QStringLiteral("url")).toString().trimmed()},
    };

    const QString id = normalizedPlaylistIdentity(item);
    if (!id.isEmpty())
    {
        payload.insert(QStringLiteral("id"), id);
    }

    const QString name = currentEpisodeDisplayName(item, index);
    if (!name.isEmpty())
    {
        payload.insert(QStringLiteral("name"), name);
    }

    const QString start = resolvedPlaylistStart(item, index);
    if (!start.isEmpty())
    {
        payload.insert(QStringLiteral("start"), start);
    }

    return payload;
}

QVariantMap VideoPlayerView::currentEpisodeProgressPayload() const
{
    if (m_playlistIndex < 0 || m_playlistIndex >= m_playlistItems.size())
    {
        return {};
    }

    const QVariantMap item = playlistItemAt(m_playlistIndex);
    QVariantMap payload = episodePayloadForItem(item, m_playlistIndex);
    const QString key = playlistProgressKey(item, m_playlistIndex);

    PlaylistProgressEntry entry;
    if (!key.isEmpty())
    {
        entry = m_playlistProgressByKey.value(key);
    }

    const double position = entry.positionSeconds > 0.0 ? entry.positionSeconds : timePos();
    const double totalDuration = entry.durationSeconds > 0.0 ? entry.durationSeconds : duration();
    payload.insert(QStringLiteral("position"), position);
    payload.insert(QStringLiteral("duration"), totalDuration);
    payload.insert(QStringLiteral("progress"), totalDuration > 0.0 ? position / totalDuration : 0.0);
    return payload;
}

QString VideoPlayerView::resolvedPlaylistStart(const QVariantMap &item, int index) const
{
    const QString key = playlistProgressKey(item, index);
    if (!key.isEmpty())
    {
        const auto it = m_playlistProgressByKey.constFind(key);
        if (it != m_playlistProgressByKey.constEnd() && it->positionSeconds > 0.0)
        {
            return QString::number(it->positionSeconds, 'f', 3);
        }
    }

    return normalizedPlaylistStart(item);
}

QString VideoPlayerView::normalizedPlaylistStart(const QVariantMap &item) const
{
    const QVariant start = item.value(QStringLiteral("start"));
    if (!start.isValid() || start.isNull())
    {
        return {};
    }

    switch (start.typeId())
    {
    case QMetaType::QString:
        return start.toString().trimmed();
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Bool:
        return start.toString().trimmed();
    default:
        return {};
    }
}

QVariantList VideoPlayerView::normalizedSubtitles(const QVariantList &subtitles) const
{
    QVariantList normalized;
    for (const QVariant &entry : subtitles)
    {
        const QVariantMap subtitle = entry.toMap();
        const QString url = subtitle.value(QStringLiteral("url")).toString().trimmed();
        const QString id = subtitle.value(QStringLiteral("id")).toString().trimmed();
        if (url.isEmpty() || id.isEmpty())
        {
            continue;
        }

        QVariantMap item = subtitle;
        item.insert(QStringLiteral("url"), url);
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("name"), subtitle.value(QStringLiteral("name")).toString().trimmed());
        item.insert(QStringLiteral("lang"), subtitle.value(QStringLiteral("lang")).toString().trimmed());
        item.insert(QStringLiteral("format"), subtitle.value(QStringLiteral("format")).toString().trimmed());
        item.insert(QStringLiteral("default"), subtitle.value(QStringLiteral("default")).toBool());
        normalized.append(item);
    }

    return normalized;
}

void VideoPlayerView::setPlaylistIndexInternal(int index)
{
    const int normalizedIndex = (index >= 0 && index < m_playlistItems.size()) ? index : -1;
    if (m_playlistIndex == normalizedIndex)
    {
        return;
    }

    m_playlistIndex = normalizedIndex;
    emit playlistIndexChanged();
}

void VideoPlayerView::loadEpisodeAtIndex(int index, const QString &reason)
{
    const QVariantMap item = playlistItemAt(index);
    const QString path = item.value(QStringLiteral("url")).toString().trimmed();
    if (path.isEmpty())
    {
        return;
    }

    QVariantMap previousEpisode = currentEpisodeProgressPayload();
    persistCurrentEpisodeProgress();
    setPlaylistIndexInternal(index);
    const QString start = resolvedPlaylistStart(item, index);
    m_session->setStartupPosition(start);
    QVariantMap nextEpisode = episodePayloadForItem(item, index);
    if (!start.isEmpty())
    {
        nextEpisode.insert(QStringLiteral("start"), start);
    }
    emit episodeSwitched(QVariantMap{
        {QStringLiteral("reason"), reason},
        {QStringLiteral("from"), previousEpisode},
        {QStringLiteral("to"), nextEpisode},
    });
    loadMedia(path, normalizedSubtitles(item.value(QStringLiteral("subtitles")).toList()));
}

mpv_handle *VideoPlayerView::mpvHandle() const
{
    return m_session->handle();
}

void VideoPlayerView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() == oldGeometry.size())
    {
        return;
    }

    scheduleWindowUpdate();
}

void VideoPlayerView::scheduleWindowUpdate()
{
    if (m_windowUpdateScheduled)
    {
        return;
    }

    m_windowUpdateScheduled = true;
    QMetaObject::invokeMethod(this, &VideoPlayerView::performWindowUpdate, Qt::QueuedConnection);
}

void VideoPlayerView::performWindowUpdate()
{
    m_windowUpdateScheduled = false;

    if (QQuickWindow *targetWindow = window())
    {
        targetWindow->update();
    }
}

void VideoPlayerView::attachToWindow(PlayerWindow *window)
{
    if (!window)
    {
        return;
    }

    m_registeredWindow = window;
    m_registeredWindow->setVideoView(this);
}

void VideoPlayerView::detachFromWindow()
{
    if (!m_registeredWindow)
    {
        return;
    }

    PlayerWindow *window = m_registeredWindow;
    m_registeredWindow = nullptr;
    m_renderContextReady = false;
    window->clearVideoView(this);
}
