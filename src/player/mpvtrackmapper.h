#ifndef LZC_PLAYER_MPVTRACKMAPPER_H_
#define LZC_PLAYER_MPVTRACKMAPPER_H_

#include <QString>
#include <QVariant>

struct MpvMappedTracks
{
    QVariantList videoTracks;
    QVariantList subtitleTracks;
    int selectedVideoId = 0;
};

class MpvTrackMapper
{
public:
    static MpvMappedTracks mapTracks(const QVariantList &trackList);
    static QString qualityLabelForVideoId(const QVariantList &videoTracks, int videoId);
};

#endif
