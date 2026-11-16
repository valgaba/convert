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

AudioOptions normalizedAudio(const QString &format, AudioOptions options)
{
    if (!QList<int>{64, 96, 128, 160, 192, 224, 256, 320}.contains(options.bitrate)) options.bitrate = 192;
    if (!QList<int>{0, 32000, 44100, 48000, 88200, 96000}.contains(options.sampleRate)) options.sampleRate = 0;
    if (format == "opus") options.sampleRate = 48000;
    else if (format == "mp3" && options.sampleRate > 48000) options.sampleRate = 48000;
    if (options.channels < 0 || options.channels > 2) options.channels = 0;
    if (!QList<int>{16, 24, 32}.contains(options.bitDepth)) options.bitDepth = 16;
    if (format == "flac" && options.bitDepth == 32) options.bitDepth = 24;
    return options;
}

QStringList audioArguments(const QString &format, AudioOptions options)
{
    options = normalizedAudio(format, options);
    QStringList args;
    if (format == "wav") args << "-c:a" << QString("pcm_s%1le").arg(options.bitDepth);
    else if (format == "flac")
        args << "-c:a" << "flac" << "-sample_fmt" << (options.bitDepth == 16 ? "s16" : "s32")
             << "-bits_per_raw_sample" << QString::number(options.bitDepth);
    else {
        const QString codec = format == "mp3" ? "libmp3lame" : format == "m4a" ? "aac"
                            : format == "opus" ? "libopus" : "libvorbis";
        args << "-c:a" << codec << "-b:a" << QString::number(options.bitrate) + "k";
    }
    if (options.sampleRate) args << "-ar" << QString::number(options.sampleRate);
    if (options.channels) args << "-ac" << QString::number(options.channels);
    return args;
}

QString audioDescription(const QString &format, AudioOptions options)
{
    options = normalizedAudio(format, options);
    return QStringLiteral("%1 · %2 · %3")
        .arg(format == "wav" || format == "flac" ? QString("%1 bits").arg(options.bitDepth)
                                                 : QString("%1 kb/s").arg(options.bitrate))
        .arg(options.sampleRate ? QString("%1 Hz").arg(options.sampleRate) : QStringLiteral("Muestreo automático"))
        .arg(options.channels == 1 ? QStringLiteral("Mono") : options.channels == 2 ? QStringLiteral("Estéreo") : QStringLiteral("Canales originales"));
}

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

QStringList conversionArguments(const QString &input, const QString &output, const QString &format,
                                AudioOptions options, bool overwriteReservedFile)
{
    QStringList args{"-hide_banner", "-nostdin", overwriteReservedFile ? "-y" : "-n", "-i", input,
                     "-progress", "pipe:1", "-nostats"};
    if (isAudio(format)) {
        args << "-map" << "0:a:0" << "-vn";
        args << audioArguments(format, options);
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
    if (role == Qt::UserRole) return int(j.kind);
    if (role == Qt::EditRole && i.column() == 2) return j.format;
    if (role == Qt::ToolTipRole && i.column() == 2)
        return QString("Pulsa para elegir el formato de salida de esta entrada.")
            + (isAudio(j.format) ? "\n" + audioDescription(j.format, j.audio)
               + "\nBotón derecho → Opciones de audio…" : QString());
    if (role == Qt::ToolTipRole) return j.source + "\n" + j.detail + "\n" + j.result;
    if (role != Qt::DisplayRole) return {};
    switch (i.column()) {
    case 0: return j.kind == Source::File ? QFileInfo(j.source).fileName() : j.source;
    case 1: return QStringList{"Archivo", "YouTube", "Spotify"}.at(int(j.kind));
    case 2: return j.format.toUpper() + " ▾";
    case 3: return QStringList{"Pendiente", "Procesando", "Completado", "Error", "Cancelado"}.at(int(j.state));
    case 4: return j.state == State::Running && j.progress < 0 ? "En curso…" : QString::number(j.progress) + " %";
    case 5: return j.result.isEmpty() ? j.destination : j.result;
    }
    return {};
}

Qt::ItemFlags JobModel::flags(const QModelIndex &i) const
{
    auto result = QAbstractTableModel::flags(i);
    if (i.isValid() && i.row() < jobs.size() && i.column() == 2 && !m_processing &&
        jobs[i.row()].state != State::Running && jobs[i.row()].state != State::Done)
        result |= Qt::ItemIsEditable;
    return result;
}

bool JobModel::setData(const QModelIndex &i, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !(flags(i) & Qt::ItemIsEditable)) return false;
    auto &job = jobs[i.row()];
    const auto format = value.toString().toLower();
    if (!formats(job.kind == Source::Spotify).contains(format)) return false;
    if (job.format == format) return true;
    job.format = format;
    job.state = State::Pending;
    job.progress = 0;
    job.detail.clear();
    job.result.clear();
    emit dataChanged(index(i.row(), 0), index(i.row(), 5),
                     {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole});
    return true;
}

void JobModel::setProcessing(bool processing)
{
    m_processing = processing;
    if (!jobs.isEmpty()) emit dataChanged(index(0, 2), index(jobs.size() - 1, 2));
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
void JobModel::clear()
{
    if (m_processing || jobs.isEmpty()) return;
    beginRemoveRows({}, 0, jobs.size() - 1);
    jobs.clear();
    endRemoveRows();
}
void JobModel::refresh(int row) { emit dataChanged(index(row, 0), index(row, 5)); }
