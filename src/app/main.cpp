#include "src/player/mpvplayerview.h"

#include <clocale>
#include <initializer_list>

#include <QColor>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickWindow>

namespace {

void setApplicationMetadata()
{
    QCoreApplication::setApplicationName(QStringLiteral("lzc-player"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(LZC_PLAYER_VERSION));
}

void configureCommandLineParser(QCommandLineParser &parser)
{
    const QCommandLineOption cookieOption(
        QStringLiteral("cookie"),
        QStringLiteral("HTTP Cookie header value to send with media requests."),
        QStringLiteral("cookie"));
    const QCommandLineOption playlistFileOption(
        QStringLiteral("playlist-file"),
        QStringLiteral("Path to a JSON playlist file."),
        QStringLiteral("file"));
    const QCommandLineOption playlistJsonOption(
        QStringLiteral("playlist-json"),
        QStringLiteral("Inline JSON playlist payload."),
        QStringLiteral("json"));
    const QCommandLineOption startOption(
        QStringLiteral("start"),
        QStringLiteral("Seek to given position on startup. Supports percent, seconds, or timestamps like 12:34 and 01:02:03."),
        QStringLiteral("time"));

    parser.setApplicationDescription(
        QStringLiteral("Lzc video player\n"
                       "\n"
                       "Examples:\n"
                       "  lzc-player --start=90 <file>\n"
                       "  lzc-player --start=12:34 <file>\n"
                       "  lzc-player --start=50% <file>\n"
                       "  lzc-player --playlist-file=episodes.json"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(cookieOption);
    parser.addOption(playlistFileOption);
    parser.addOption(playlistJsonOption);
    parser.addOption(startOption);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Media file or URL to open."));
}

bool hasCommandLineOption(int argc, char **argv, std::initializer_list<const char *> names)
{
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        for (const char *name : names) {
            if (argument == QString::fromLatin1(name)) {
                return true;
            }
        }
    }

    return false;
}

QVariantList parsePlaylistPayload(const QByteArray &payload, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (document.isNull())
    {
        if (error)
        {
            *error = parseError.errorString();
        }
        return {};
    }

    if (!document.isArray())
    {
        if (error)
        {
            *error = QStringLiteral("playlist payload must be a JSON array");
        }
        return {};
    }

    return document.array().toVariantList();
}

QVariantList loadPlaylistFromParser(const QCommandLineParser &parser)
{
    QString error;
    if (parser.isSet(QStringLiteral("playlist-file")))
    {
        QFile file(parser.value(QStringLiteral("playlist-file")));
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning().noquote() << "Failed to open playlist file:" << file.fileName();
            return {};
        }

        const QVariantList playlist = parsePlaylistPayload(file.readAll(), &error);
        if (!error.isEmpty())
        {
            qWarning().noquote() << "Failed to parse playlist file JSON:" << error;
        }
        return playlist;
    }

    if (parser.isSet(QStringLiteral("playlist-json")))
    {
        const QVariantList playlist = parsePlaylistPayload(
            parser.value(QStringLiteral("playlist-json")).toUtf8(), &error);
        if (!error.isEmpty())
        {
            qWarning().noquote() << "Failed to parse playlist JSON:" << error;
        }
        return playlist;
    }

    return {};
}

} // namespace

int main(int argc, char **argv)
{
    if (hasCommandLineOption(argc, argv, {"--help", "--help-all", "-h", "-?"})) {
        QCoreApplication app(argc, argv);
        setApplicationMetadata();

        QCommandLineParser parser;
        configureCommandLineParser(parser);
        parser.process(app);
    }

    if (hasCommandLineOption(argc, argv, {"--version", "-v"})) {
        QCoreApplication app(argc, argv);
        setApplicationMetadata();

        QCommandLineParser parser;
        configureCommandLineParser(parser);
        parser.process(app);
    }

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);
    setApplicationMetadata();

    QCommandLineParser parser;
    configureCommandLineParser(parser);
    parser.process(app);

    const QVariantList startupPlaylist = loadPlaylistFromParser(parser);
    const QStringList positionalArguments = parser.positionalArguments();
    const QString startupFile = startupPlaylist.isEmpty() && !positionalArguments.isEmpty()
        ? positionalArguments.first()
        : QString();
    const QString startupPosition = parser.value(QStringLiteral("start"));
    app.setProperty("lzcPlayerCookie", parser.value(QStringLiteral("cookie")));

    std::setlocale(LC_NUMERIC, "C");

    qmlRegisterType<MpvPlayerView>("mpvtest", 1, 0, "MpvPlayerView");

    QQuickView view;
    view.setColor(QColor(QStringLiteral("#000000")));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.rootContext()->setContextProperty("initialFile", startupFile);
    view.rootContext()->setContextProperty("initialPlaylist", startupPlaylist);
    view.rootContext()->setContextProperty("initialStartPosition", startupPosition);
    view.setSource(QUrl("qrc:/lzc-player/qml/Main.qml"));
    view.show();

    return app.exec();
}
