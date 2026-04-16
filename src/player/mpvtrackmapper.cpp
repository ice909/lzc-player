#include "src/player/mpvtrackmapper.h"

#include <algorithm>

namespace
{

QString formatSubtitleTitle(QString title, const QString &lang)
{
    title = title.trimmed();
    if (title.isEmpty())
    {
        return title;
    }

    title.remove(QStringLiteral("HDMV_PGS_SUBTITLE"));
    title.remove(QStringLiteral("PGS"));
    title.remove(QStringLiteral("SUP"));
    title.remove(QChar('/'));
    title = title.simplified();

    const QString upperLang = lang.trimmed().toUpper();
    if (!upperLang.isEmpty())
    {
        title.remove(upperLang, Qt::CaseInsensitive);
        title = title.simplified();
    }

    if (title.isEmpty())
    {
        if (upperLang == QStringLiteral("CHS"))
        {
            return QStringLiteral("简体中文");
        }
        if (upperLang == QStringLiteral("CHT"))
        {
            return QStringLiteral("繁体中文");
        }
        if (upperLang == QStringLiteral("CHS/ENG"))
        {
            return QStringLiteral("简英字幕");
        }
        if (upperLang == QStringLiteral("CHT/ENG"))
        {
            return QStringLiteral("繁英字幕");
        }
    }

    return title;
}

QString formatSubtitleCodec(const QString &codec)
{
    const QString upper = codec.trimmed().toUpper();
    if (upper.contains(QStringLiteral("PGS")) || upper.contains(QStringLiteral("HDMV_PGS")))
    {
        return QStringLiteral("SUP");
    }
    if (upper.contains(QStringLiteral("SUBRIP")) || upper == QStringLiteral("SRT"))
    {
        return QStringLiteral("SRT");
    }
    if (upper.contains(QStringLiteral("ASS")))
    {
        return QStringLiteral("ASS");
    }
    if (upper.contains(QStringLiteral("SSA")))
    {
        return QStringLiteral("SSA");
    }
    if (upper.contains(QStringLiteral("WEBVTT")) || upper == QStringLiteral("VTT"))
    {
        return QStringLiteral("VTT");
    }
    return upper;
}

} // namespace

MpvMappedTracks MpvTrackMapper::mapTracks(const QVariantList &trackList)
{
    MpvMappedTracks mapped;
    mapped.subtitleTracks = QVariantList{QVariantMap{
        {QStringLiteral("id"), 0},
        {QStringLiteral("title"), QStringLiteral("关闭")},
        {QStringLiteral("detail"), QString()},
        {QStringLiteral("isDefault"), false},
    }};

    for (const auto &entry : trackList)
    {
        const auto track = entry.toMap();
        const QString type = track.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("video"))
        {
            const int id = track.value(QStringLiteral("id")).toInt();
            const int width = track.value(QStringLiteral("demux-w")).toInt();
            const int height = track.value(QStringLiteral("demux-h")).toInt();
            const qint64 bitrate = track.value(QStringLiteral("demux-bitrate")).toLongLong();
            const qint64 hlsBitrate = track.value(QStringLiteral("hls-bitrate")).toLongLong();
            const qint64 effectiveBitrate = bitrate > 0 ? bitrate : hlsBitrate;
            if (track.value(QStringLiteral("selected")).toBool())
            {
                mapped.selectedVideoId = id;
            }

            QString label;
            if (height >= 2160 || width >= 3840)
            {
                label = QStringLiteral("4K");
            }
            else if (height > 0)
            {
                label = QStringLiteral("%1P").arg(height);
            }
            else
            {
                label = QStringLiteral("原画质");
            }

            QString detail = label;
            if (effectiveBitrate > 0)
            {
                detail += QStringLiteral(" %1Mbps").arg(
                    QString::number(double(effectiveBitrate) / 1000000.0, 'f', 0));
            }

            mapped.videoTracks.append(QVariantMap{
                {QStringLiteral("id"), id},
                {QStringLiteral("label"), label},
                {QStringLiteral("detail"), detail},
                {QStringLiteral("width"), width},
                {QStringLiteral("height"), height},
                {QStringLiteral("bitrate"), effectiveBitrate},
            });
            continue;
        }

        if (type != QStringLiteral("sub"))
        {
            continue;
        }

        const int id = track.value(QStringLiteral("id")).toInt();
        const QString lang = track.value(QStringLiteral("lang")).toString();
        const QString codec = formatSubtitleCodec(track.value(QStringLiteral("codec")).toString());
        const bool isDefault = track.value(QStringLiteral("default")).toBool();
        QString title = formatSubtitleTitle(track.value(QStringLiteral("title")).toString(), lang);
        if (title.isEmpty())
        {
            title = QStringLiteral("字幕 %1").arg(id);
        }

        QString detail = codec;
        if (!lang.isEmpty())
        {
            detail += detail.isEmpty() ? lang.toUpper() : QStringLiteral(" %1").arg(lang.toUpper());
        }

        mapped.subtitleTracks.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("title"), title},
            {QStringLiteral("detail"), detail},
            {QStringLiteral("isDefault"), isDefault},
        });
    }

    std::sort(mapped.videoTracks.begin(), mapped.videoTracks.end(), [](const QVariant &lhs, const QVariant &rhs) {
        const auto left = lhs.toMap();
        const auto right = rhs.toMap();

        const qint64 leftBitrate = left.value(QStringLiteral("bitrate")).toLongLong();
        const qint64 rightBitrate = right.value(QStringLiteral("bitrate")).toLongLong();
        if (leftBitrate != rightBitrate)
        {
            return leftBitrate > rightBitrate;
        }

        const int leftHeight = left.value(QStringLiteral("height")).toInt();
        const int rightHeight = right.value(QStringLiteral("height")).toInt();
        if (leftHeight != rightHeight)
        {
            return leftHeight > rightHeight;
        }

        return left.value(QStringLiteral("width")).toInt()
            > right.value(QStringLiteral("width")).toInt();
    });

    if (!mapped.videoTracks.isEmpty())
    {
        auto first = mapped.videoTracks.first().toMap();
        first.insert(QStringLiteral("label"), QStringLiteral("原画质"));
        mapped.videoTracks[0] = first;
    }

    return mapped;
}

QString MpvTrackMapper::qualityLabelForVideoId(const QVariantList &videoTracks, int videoId)
{
    const int normalizedId = videoId > 0 ? videoId : 0;
    QString label = QStringLiteral("原画质");
    for (const auto &entry : videoTracks)
    {
        const auto track = entry.toMap();
        if (track.value(QStringLiteral("id")).toInt() == normalizedId)
        {
            label = track.value(QStringLiteral("label")).toString();
            break;
        }
    }

    return label;
}
