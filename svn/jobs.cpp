#include "jobs.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

QStringList formats(bool audioOnly)
{
    QStringList result{"mp3", "wav", "flac", "m4a", "opus", "ogg"};
    if (!audioOnly) result = QStringList{"mp4", "mkv", "webm"} + result;
    return result;
}

bool isAudio(const QString &format) { return formats(true).contains(format); }

QString outputDirectory(const QString &text)
{
    QString path = text.trimmed();
    if (path.size() >= 2 && path.startsWith('"') && path.endsWith('"'))
        path = path.mid(1, path.size() - 2).trimmed();
    if (path.isEmpty()) return {};
    const QUrl url(path);
    if (url.isLocalFile()) path = url.toLocalFile();
    path = QDir::fromNativeSeparators(path);
    if (QDir::isRelativePath(path))
        path = QDir(QCoreApplication::applicationDirPath()).filePath(path);
    return QDir::cleanPath(path);
}

QString toolPath(const QString &name)
{
    const auto configured = QSettings().value("tools/" + name).toString();
    if (!configured.isEmpty()) return configured;
    QString executable = name;
#ifdef Q_OS_WIN
    executable += ".exe";
#endif
    const QDir base(QCoreApplication::applicationDirPath());
    for (const auto &folder : QStringList{"bin", "ffmpeg", "tools", "."}) {
        const auto candidate = base.filePath(folder + "/" + executable);
        if (QFileInfo(candidate).isFile()) return candidate;
    }
    return QStandardPaths::findExecutable(executable);
}

bool classifyUrl(const QString &text, Source *kind)
{
    const QUrl url(text);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")
        || !url.userInfo().isEmpty()) return false;
    const auto host = url.host().toLower();
    if (host == "youtu.be" || host == "youtube.com" || host.endsWith(".youtube.com")) {
        *kind = Source::YouTube;
        return true;
    }
    if (host == "open.spotify.com") {
        const auto parts = url.path().split('/', Qt::SkipEmptyParts);
        const int offset = !parts.isEmpty() && parts.first().startsWith("intl-") ? 1 : 0;
        if (parts.size() != offset + 2 ||
            !QStringList{"track", "album", "playlist"}.contains(parts[offset])) return false;
        *kind = Source::Spotify;
        return true;
    }
    return false;
}

QStringList conversionArguments(const QString &input, const QString &output, const QString &format)
{
    QStringList args{"-hide_banner", "-nostdin", "-n", "-i", input,
                     "-progress", "pipe:1", "-nostats"};
    if (isAudio(format)) {
        args << "-map" << "0:a:0" << "-vn";
        if (format == "mp3") args << "-c:a" << "libmp3lame" << "-b:a" << "192k";
        else if (format == "wav") args << "-c:a" << "pcm_s16le";
        else if (format == "flac") args << "-c:a" << "flac";
        else if (format == "m4a") args << "-c:a" << "aac" << "-b:a" << "192k";
        else if (format == "opus") args << "-c:a" << "libopus" << "-b:a" << "160k";
        else args << "-c:a" << "libvorbis" << "-q:a" << "5";
    } else {
        args << "-map" << "0:v:0" << "-map" << "0:a:0?";
        if (format == "webm")
            args << "-c:v" << "libvpx-vp9" << "-crf" << "32" << "-b:v" << "0"
                 << "-c:a" << "libopus";
        else {
            args << "-c:v" << "libx264" << "-preset" << "medium" << "-crf" << "23"
                 << "-vf" << "pad=ceil(iw/2)*2:ceil(ih/2)*2" << "-pix_fmt" << "yuv420p"
                 << "-c:a" << "aac" << "-b:a" << "192k";
            if (format == "mp4") args << "-movflags" << "+faststart";
        }
    }
    return args << output;
}

JobModel::JobModel(QObject *parent) : QAbstractTableModel(parent) {}
int JobModel::rowCount(const QModelIndex &p) const { return p.isValid() ? 0 : jobs.size(); }
int JobModel::columnCount(const QModelIndex &p) const { return p.isValid() ? 0 : 6; }
QVariant JobModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    return QStringList{"Archivo / enlace", "Origen", "Formato", "Estado", "Progreso", "Destino"}.value(section);
}
QVariant JobModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.row() >= jobs.size()) return {};
    const auto &j = jobs[i.row()];
    if (role == Qt::ToolTipRole) return j.source + "\n" + j.detail + "\n" + j.result;
    if (role != Qt::DisplayRole) return {};
    switch (i.column()) {
    case 0: return j.kind == Source::File ? QFileInfo(j.source).fileName() : j.source;
    case 1: return QStringList{"Archivo", "YouTube", "Spotify"}.at(int(j.kind));
    case 2: return j.format.toUpper();
    case 3: return QStringList{"Pendiente", "Procesando", "Completado", "Error", "Cancelado"}.at(int(j.state));
    case 4: return j.state == State::Running && j.progress < 0 ? "En curso…" : QString::number(j.progress) + " %";
    case 5: return j.result.isEmpty() ? j.destination : j.result;
    }
    return {};
}
void JobModel::append(const Job &j)
{
    const int row = jobs.size();
    beginInsertRows({}, row, row); jobs.append(j); endInsertRows();
}
void JobModel::remove(int row)
{
    beginRemoveRows({}, row, row); jobs.removeAt(row); endRemoveRows();
}
void JobModel::refresh(int row) { emit dataChanged(index(row, 0), index(row, 5)); }
