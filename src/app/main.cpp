#include "src/app/playerwindow.h"
#include "src/player/mpvplayerview.h"

#include <clocale>
#include <initializer_list>

#include <QApplication>
#include <QColor>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickWindow>

namespace {

struct StartupOptions
{
    QString file;
    QVariantList playlist;
    QString start;
    QString cookie;
    QString inputIpcServer;
};

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
    const QCommandLineOption inputIpcServerOption(
        QStringLiteral("input-ipc-server"),
        QStringLiteral("Path to the mpv JSON IPC server socket."),
        QStringLiteral("path"));
    const QCommandLineOption configFileOption(
        QStringList{QStringLiteral("f"), QStringLiteral("config-file")},
        QStringLiteral("Path to a JSON config file. Command line arguments override config values."),
        QStringLiteral("file"));

    parser.setApplicationDescription(
        QStringLiteral("Lzc video player\n"
                       "\n"
                       "Examples:\n"
                       "  lzc-player --start=90 <file>\n"
                       "  lzc-player --start=12:34 <file>\n"
                       "  lzc-player --start=50% <file>\n"
                       "  lzc-player --playlist-file=episodes.json\n"
                       "  lzc-player -f player.json"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(configFileOption);
    parser.addOption(cookieOption);
    parser.addOption(playlistFileOption);
    parser.addOption(playlistJsonOption);
    parser.addOption(startOption);
    parser.addOption(inputIpcServerOption);
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

QString resolveConfigPath(const QString &path, const QString &baseDir)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path) || path.contains(QStringLiteral("://")))
    {
        return path;
    }

    return QDir(baseDir).filePath(path);
}

bool readJsonString(const QJsonObject &object, std::initializer_list<const char *> names, QString *value)
{
    for (const char *name : names)
    {
        const QString key = QString::fromLatin1(name);
        if (!object.contains(key) || object.value(key).isNull())
        {
            continue;
        }

        const QJsonValue jsonValue = object.value(key);
        if (jsonValue.isString())
        {
            *value = jsonValue.toString();
            return true;
        }

        if (jsonValue.isDouble() || jsonValue.isBool())
        {
            *value = jsonValue.toVariant().toString();
            return true;
        }

        qWarning().noquote() << "Ignoring config value with unsupported type:" << key;
        return false;
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

QVariantList parsePlaylistConfigValue(const QJsonValue &value, QString *error)
{
    if (value.isArray())
    {
        return value.toArray().toVariantList();
    }

    if (value.isString())
    {
        return parsePlaylistPayload(value.toString().toUtf8(), error);
    }

    if (error)
    {
        *error = QStringLiteral("playlist config value must be a JSON array or JSON string");
    }
    return {};
}

QVariantList loadPlaylistFromFile(const QString &fileName)
{
    QString error;
    QFile file(fileName);
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

QVariantList loadPlaylistFromJson(const QString &json)
{
    QString error;
    const QVariantList playlist = parsePlaylistPayload(json.toUtf8(), &error);
    if (!error.isEmpty())
    {
        qWarning().noquote() << "Failed to parse playlist JSON:" << error;
    }
    return playlist;
}

bool loadStartupOptionsFromConfigFile(const QString &fileName, StartupOptions *options)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        qCritical().noquote() << "Failed to open config file:" << file.fileName();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (document.isNull())
    {
        qCritical().noquote() << "Failed to parse config file JSON:" << parseError.errorString();
        return false;
    }

    if (!document.isObject())
    {
        qCritical().noquote() << "Config file JSON must be an object:" << file.fileName();
        return false;
    }

    const QFileInfo configFileInfo(fileName);
    const QString configDir = configFileInfo.absoluteDir().absolutePath();
    const QJsonObject object = document.object();

    QString filePath;
    if (readJsonString(object, {"file"}, &filePath))
    {
        options->file = resolveConfigPath(filePath.trimmed(), configDir);
    }

    readJsonString(object, {"start"}, &options->start);
    readJsonString(object, {"cookie"}, &options->cookie);
    readJsonString(object, {"inputIpcServer", "input-ipc-server"}, &options->inputIpcServer);

    QString playlistFileName;
    if (readJsonString(object, {"playlistFile", "playlist-file"}, &playlistFileName))
    {
        options->playlist = loadPlaylistFromFile(resolveConfigPath(playlistFileName.trimmed(), configDir));
    }

    if (object.contains(QStringLiteral("playlist")))
    {
        QString error;
        const QVariantList playlist = parsePlaylistConfigValue(object.value(QStringLiteral("playlist")), &error);
        if (!error.isEmpty())
        {
            qWarning().noquote() << "Failed to parse config playlist:" << error;
        }
        else
        {
            options->playlist = playlist;
        }
    }

    if (object.contains(QStringLiteral("playlistJson")) || object.contains(QStringLiteral("playlist-json")))
    {
        const QJsonValue value = object.contains(QStringLiteral("playlistJson"))
            ? object.value(QStringLiteral("playlistJson"))
            : object.value(QStringLiteral("playlist-json"));
        QString error;
        const QVariantList playlist = parsePlaylistConfigValue(value, &error);
        if (!error.isEmpty())
        {
            qWarning().noquote() << "Failed to parse config playlistJson:" << error;
        }
        else
        {
            options->playlist = playlist;
        }
    }

    return true;
}

bool loadStartupOptionsFromParser(const QCommandLineParser &parser, StartupOptions *options)
{
    if (parser.isSet(QStringLiteral("config-file")))
    {
        if (!loadStartupOptionsFromConfigFile(parser.value(QStringLiteral("config-file")), options))
        {
            return false;
        }
    }

    if (parser.isSet(QStringLiteral("playlist-file")))
    {
        options->playlist = loadPlaylistFromFile(parser.value(QStringLiteral("playlist-file")));
    }
    else if (parser.isSet(QStringLiteral("playlist-json")))
    {
        options->playlist = loadPlaylistFromJson(parser.value(QStringLiteral("playlist-json")));
    }

    const QStringList positionalArguments = parser.positionalArguments();
    if (!positionalArguments.isEmpty())
    {
        options->file = positionalArguments.first();
    }

    if (parser.isSet(QStringLiteral("start")))
    {
        options->start = parser.value(QStringLiteral("start"));
    }

    if (parser.isSet(QStringLiteral("cookie")))
    {
        options->cookie = parser.value(QStringLiteral("cookie"));
    }

    if (parser.isSet(QStringLiteral("input-ipc-server")))
    {
        options->inputIpcServer = parser.value(QStringLiteral("input-ipc-server"));
    }

    return true;
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
    QApplication app(argc, argv);
    setApplicationMetadata();

    QCommandLineParser parser;
    configureCommandLineParser(parser);
    parser.process(app);

    StartupOptions startupOptions;
    if (!loadStartupOptionsFromParser(parser, &startupOptions))
    {
        return 1;
    }

    const QString startupFile = startupOptions.playlist.isEmpty() ? startupOptions.file : QString();
    app.setProperty("lzcPlayerCookie", startupOptions.cookie);
    app.setProperty("lzcPlayerInputIpcServer", startupOptions.inputIpcServer);

    std::setlocale(LC_NUMERIC, "C");

    qmlRegisterType<MpvPlayerView>("mpvtest", 1, 0, "MpvPlayerView");

    PlayerWindow view;
    view.setColor(QColor(QStringLiteral("#000000")));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.rootContext()->setContextProperty("playerWindow", &view);
    view.rootContext()->setContextProperty("initialFile", startupFile);
    view.rootContext()->setContextProperty("initialPlaylist", startupOptions.playlist);
    view.rootContext()->setContextProperty("initialStartPosition", startupOptions.start);
    view.setSource(QUrl("qrc:/lzc-player/qml/Main.qml"));
    view.show();

    return app.exec();
}
