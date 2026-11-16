#include "runner.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QSet>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#endif

Runner::Runner(JobModel *model, QObject *parent) : QObject(parent), m_model(model) {}
Runner::~Runner()
{
    closeProcessTree();
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1500);
    }
    if ((!m_process || m_process->state() == QProcess::NotRunning) && !m_staged.isEmpty())
        QFile::remove(m_staged);
}

void Runner::start()
{
    if (m_busy) return;
    m_busy = true;
    m_cancelled = false;
    emit busyChanged(true);
    next();
}

void Runner::cancel()
{
    if (!m_busy) return;
    m_cancelled = true;
    if (m_process) {
        closeProcessTree();
        m_process->kill();
    }
}

void Runner::closeProcessTree()
{
#ifdef Q_OS_WIN
    if (m_processTree) CloseHandle(static_cast<HANDLE>(m_processTree));
#endif
    m_processTree = nullptr;
}

void Runner::next()
{
    m_row = -1;
    if (!m_cancelled) {
        for (int i = 0; i < m_model->jobs.size(); ++i) {
            if (m_model->jobs[i].state == State::Pending) { m_row = i; break; }
        }
    }
    if (m_row < 0) {
        m_busy = false;
        emit busyChanged(false);
        return;
    }
    auto &j = m_model->jobs[m_row];
    j.state = State::Running;
    j.progress = -1;
    j.detail.clear();
    m_model->refresh(m_row);
    emit checkpoint();
    m_duration = 0;
    m_output.clear();
    m_staged.clear();
    m_downloadFiles.clear();
    m_encodingDownload = false;
    m_downloadIndex = 0;
    m_encodedDownload.clear();
    m_spotifyMetadata.clear();
    m_spotifyErrors.clear();
    m_downloadTemp.reset();
    j.destination = outputDirectory(j.destination);
    const auto displayDestination = QDir::toNativeSeparators(j.destination);
    emit log("Destino: " + displayDestination);
    if (isAudio(j.format)) emit log("Audio: " + audioDescription(j.format, j.audio));
    if (j.destination.isEmpty()) {
        complete(false, "La carpeta de destino está vacía. Selecciona una carpeta y pulsa Aplicar a selección."); return;
    }
    if (QFileInfo::exists(j.destination) && !QFileInfo(j.destination).isDir()) {
        complete(false, "El destino es un archivo, no una carpeta: " + displayDestination); return;
    }
    if (!QDir().mkpath(j.destination)) {
        complete(false, "No se puede crear la carpeta de destino: " + displayDestination
                 + ". Revisa la ruta y los permisos; después pulsa Aplicar a selección."); return;
    }
    {
        QTemporaryFile writable(QDir(j.destination).filePath(".convert-write-XXXXXX"));
        if (!writable.open()) {
            complete(false, "No se puede escribir en el destino: " + displayDestination
                     + ". " + writable.errorString()
                     + ". Usa el botón derecho sobre la entrada y elige Cambiar carpeta de salida.");
            return;
        }
    }
    const auto ffmpeg = toolPath("ffmpeg");
    if (ffmpeg.isEmpty() || !QFileInfo(ffmpeg).isExecutable()) {
        complete(false, "No se encuentra FFmpeg. Revisa Preferencias."); return;
    }
    if (j.kind == Source::File) {
        if (!QFileInfo(j.source).isFile()) {
            complete(false, "El archivo de origen ya no existe."); return;
        }
        const auto base = QFileInfo(j.source).completeBaseName();
        QDir dir(j.destination);
        m_output = dir.filePath(base + "." + j.format);
        int suffix = 2;
        while (QFileInfo::exists(m_output))
            m_output = dir.filePath(base + " (" + QString::number(suffix++) + ")." + j.format);
        // A network share can allow files while denying subdirectories.
        // Reserve a unique file beside the final output, then release its
        // handle before FFmpeg opens it. Only this reserved file uses -y.
        {
            QTemporaryFile temporary(dir.filePath("convert-XXXXXX." + j.format));
            if (!temporary.open()) {
                complete(false, "No se puede crear el archivo temporal en " + displayDestination
                         + ": " + temporary.errorString());
                return;
            }
            m_staged = temporary.fileName();
            temporary.setAutoRemove(false);
        }
        emit log("Archivo temporal: " + QDir::toNativeSeparators(m_staged));
        const auto probe = toolPath("ffprobe");
        if (probe.isEmpty()) convert();
        else execute(probe, {"-v", "error", "-show_entries", "format=duration",
                             "-of", "json", j.source}, true);
        return;
    }

    // Los identificadores solo se usan en una carpeta temporal de trabajo.
    // Los archivos finales se publican con nombres legibles en el destino.
    if (!prepareProcessTemporaryDirectory()) {
        complete(false, "No se puede preparar la carpeta temporal de descarga."); return;
    }
    // Mismo volumen que la salida: el paso final es un renombrado, sin copiar
    // vídeos grandes entre discos en el hilo de la interfaz.
    // spotDL elimina puntos iniciales de los componentes de --output.
    m_downloadTemp = std::make_unique<QTemporaryDir>(QDir(j.destination).filePath("convert-download-XXXXXX"));
    if (!m_downloadTemp->isValid()) {
        complete(false, "No se puede crear la carpeta de trabajo de la descarga."); return;
    }
    const QString folder = m_downloadTemp->path();
    emit log("Carpeta de trabajo de descarga: " + QDir::toNativeSeparators(folder));
    j.result = j.destination;
    m_model->refresh(m_row);
    emit checkpoint();
    if (j.kind == Source::Spotify) {
        if (!isAudio(j.format)) { complete(false, "Spotify requiere un formato de audio."); return; }
        m_spotifyMetadata = QDir(m_runtimeTemp->path()).filePath("download.spotdl");
        m_spotifyErrors = QDir(m_runtimeTemp->path()).filePath("errors.txt");
        execute(toolPath("spotdl"), {"download", j.source, "--format", j.format,
                "--ffmpeg", ffmpeg, "--output",
                "{track-id} - {artists} - {title}.{output-ext}",
                "--bitrate", QString::number(normalizedAudio(j.format, j.audio).bitrate) + "k",
                "--ffmpeg-args=" + audioArguments(j.format, j.audio).join(' '),
                "--overwrite", "skip", "--threads", "1", "--restrict", "none",
                "--simple-tui", "--print-errors", "--save-errors",
                m_spotifyErrors, "--save-file", m_spotifyMetadata, "--no-cache"});
    } else {
        QStringList args{"--ignore-config", "--newline", "--progress", "--no-colors", "--no-overwrites",
                         "--windows-filenames", "--ffmpeg-location", QFileInfo(ffmpeg).absolutePath(),
                         "--progress-template", "download:CONVERT_PROGRESS:%(progress._percent_str)s",
                         "--print", "after_move:CONVERT_FILE:%(filepath)j", "--no-simulate",
                         "-P", folder, "-o", "%(title).160B [%(id)s].%(ext)s"};
        if (isAudio(j.format))
            args << "-f" << "bestaudio/best";
        else if (j.format == "webm")
            args << "-f" << "bv[ext=webm]+ba[ext=webm]/b[ext=webm]"
                 << "--merge-output-format" << "webm" << "--remux-video" << "webm";
        else if (j.format == "mp4")
            args << "-f" << "bv[ext=mp4]+ba[ext=m4a]/b[ext=mp4]/best"
                 << "--merge-output-format" << "mp4" << "--recode-video" << "mp4";
        else args << "-f" << "bv+ba/b" << "--merge-output-format" << "mkv"
                  << "--remux-video" << "mkv";
        args << "--" << j.source;
        execute(toolPath("yt-dlp"), args);
    }
}

void Runner::convert()
{
    if (m_cancelled) { complete(false, "Cancelado por el usuario."); return; }
    const auto &j = m_model->jobs[m_row];
    execute(toolPath("ffmpeg"), conversionArguments(j.source, m_staged, j.format, j.audio, true));
}

void Runner::convertDownloadedAudio()
{
    if (m_cancelled) { complete(false, "Cancelado por el usuario."); return; }
    auto &job = m_model->jobs[m_row];
    if (m_downloadIndex >= m_downloadFiles.size()) {
        QString error;
        if (!publishDownloads(&error)) { complete(false, error); return; }
        complete(true, "Finalizado.");
        return;
    }
    const QFileInfo input(m_downloadFiles[m_downloadIndex]);
    if (!input.isFile() || input.size() <= 0) {
        complete(false, "No se ha encontrado el audio descargado."); return;
    }
    const QDir folder(QDir(m_downloadTemp->path()).filePath("audio"));
    if (!QDir().mkpath(folder.path())) {
        complete(false, "No se puede preparar la conversión del audio descargado."); return;
    }
    m_encodingDownload = true;
    m_encodedDownload = folder.filePath(input.completeBaseName() + "." + job.format);
    job.progress = -1;
    m_model->refresh(m_row);
    emit log(QString("Convirtiendo audio %1 de %2").arg(m_downloadIndex + 1).arg(m_downloadFiles.size()));
    execute(toolPath("ffmpeg"), conversionArguments(input.absoluteFilePath(), m_encodedDownload,
                                                   job.format, job.audio));
}

bool Runner::prepareProcessTemporaryDirectory()
{
    if (m_runtimeTemp && m_runtimeTemp->isValid()) return true;

    // No depender de TEMP/TMP heredados: los ejecutables PyInstaller necesitan
    // una carpeta existente donde puedan descomprimir su entorno Python.
    QStringList roots{QDir(QCoreApplication::applicationDirPath()).filePath("temp")};
    const auto localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!localData.isEmpty()) roots << QDir(localData).filePath("temp");
    roots << QDir(QDir::tempPath()).filePath("Convert");
    roots.removeDuplicates();

    for (const auto &root : roots) {
        if (!QDir().mkpath(root)) continue;
        auto folder = std::make_unique<QTemporaryDir>(QDir(root).filePath("run-XXXXXX"));
        if (!folder->isValid()) continue;
        emit log("Carpeta temporal de herramientas: " + QDir::toNativeSeparators(folder->path()));
        m_runtimeTemp = std::move(folder);
        return true;
    }
    emit log("No se pudo crear una carpeta temporal en: " + roots.join("; "));
    return false;
}

void Runner::execute(const QString &program, const QStringList &arguments, bool probe)
{
    if (program.isEmpty() || !QFileInfo(program).isExecutable()) {
        complete(false, "Herramienta no encontrada. Configura sus rutas en Preferencias."); return;
    }
    if (!prepareProcessTemporaryDirectory()) {
        complete(false, "No se puede crear la carpeta temporal de las herramientas. Comprueba permisos y espacio libre.");
        return;
    }
    m_probe = probe;
    m_temporaryFailure = false;
    m_buffer.clear();
    m_probeData.clear();
    // Los subprocesos de yt-dlp/spotDL también deben encontrar FFmpeg y ffprobe.
    auto env = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    if (m_model->jobs[m_row].kind == Source::Spotify && !probe) {
        // spotDL crea ~/.spotdl al importar sus módulos, antes de leer --no-cache
        // o --cache-path. Python en Windows resuelve ~ mediante USERPROFILE.
        QStringList profiles{QDir(QCoreApplication::applicationDirPath()).filePath("data/spotdl-profile")};
        const auto localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!localData.isEmpty()) profiles << QDir(localData).filePath("spotdl-profile");
        QString profile;
        QString lastError;
        for (const auto &candidate : profiles) {
            const QDir dir(candidate);
            if (!QDir().mkpath(dir.filePath(".spotdl")) ||
                !QDir().mkpath(dir.filePath("AppData/Local")) ||
                !QDir().mkpath(dir.filePath("AppData/Roaming"))) {
                lastError = "No se pueden crear las carpetas de datos en " + candidate;
                continue;
            }
            QTemporaryFile writable(dir.filePath(".spotdl/.convert-write-XXXXXX"));
            if (!writable.open()) {
                lastError = candidate + ": " + writable.errorString();
                continue;
            }
            profile = candidate;
            break;
        }
        if (profile.isEmpty()) {
            complete(false, "No se pueden preparar los datos locales de spotDL. " + lastError);
            return;
        }
        // Solo afecta a spotDL y a sus hijos; no altera el perfil de Windows.
        env.insert("USERPROFILE", QDir::toNativeSeparators(profile));
        env.insert("LOCALAPPDATA", QDir::toNativeSeparators(QDir(profile).filePath("AppData/Local")));
        env.insert("APPDATA", QDir::toNativeSeparators(QDir(profile).filePath("AppData/Roaming")));
        emit log("Datos de spotDL: " + QDir::toNativeSeparators(QDir(profile).filePath(".spotdl")));
    }
#endif
    QStringList dirs;
    for (const auto &name : QStringList{"ffmpeg", "ffprobe", "yt-dlp"}) {
        const auto path = toolPath(name);
        if (!path.isEmpty()) dirs << QFileInfo(path).absolutePath();
    }
    dirs.removeDuplicates();
    dirs << env.value("PATH");
    env.insert("PATH", dirs.join(QDir::listSeparator()));
    env.insert("PYTHONIOENCODING", "utf-8");
    env.insert("NO_COLOR", "1");
    const auto runtimePath = QDir::toNativeSeparators(m_runtimeTemp->path());
    env.insert("TEMP", runtimePath);
    env.insert("TMP", runtimePath);
    env.insert("TMPDIR", runtimePath);
    auto *process = new QProcess(this);
    m_process = process;
    process->setProcessChannelMode(QProcess::MergedChannels);
    if (m_model->jobs[m_row].kind == Source::Spotify && !probe && m_downloadTemp)
        process->setWorkingDirectory(m_downloadTemp->path());
    process->setProcessEnvironment(env);
    emit log("▶ " + QFileInfo(program).fileName() + " · " + m_model->jobs[m_row].source);
    connect(process, &QProcess::started, this, [this, process] {
#ifdef Q_OS_WIN
        // KILL_ON_JOB_CLOSE también termina FFmpeg cuando se cancela una descarga.
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                                   static_cast<DWORD>(process->processId()));
        if (job && child && SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                &info, sizeof(info)) && AssignProcessToJobObject(job, child)) {
            m_processTree = job;
        } else {
            if (job) CloseHandle(job);
            emit log("Aviso: no se pudo agrupar el proceso; la cancelación solo terminará el proceso principal.");
        }
        if (child) CloseHandle(child);
#else
        Q_UNUSED(process);
#endif
        if (m_cancelled) { closeProcessTree(); m_process->kill(); }
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this] { consume(); });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        const QString message = process->errorString();
        m_process = nullptr;
        process->deleteLater();
        complete(false, message);
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process](int code, QProcess::ExitStatus status) {
        consume(true);
        closeProcessTree();
        m_process = nullptr;
        process->deleteLater();
        if (m_cancelled) { complete(false, "Cancelado por el usuario."); return; }
        if (m_probe) {
            m_duration = QJsonDocument::fromJson(m_probeData).object().value("format")
                             .toObject().value("duration").toString().toDouble();
            // Un fallo de sondeo no impide intentar la conversión.
            convert();
            return;
        }
        bool success = code == 0 && status == QProcess::NormalExit;
        if (!success && m_temporaryFailure) {
            complete(false, "La herramienta no pudo crear su directorio temporal en "
                     + QDir::toNativeSeparators(m_runtimeTemp->path())
                     + ". Comprueba permisos y espacio libre. Consulta el registro.");
            return;
        }
        auto &j = m_model->jobs[m_row];
        if (m_encodingDownload) {
            if (!success || QFileInfo(m_encodedDownload).size() <= 0) {
                complete(false, "No se pudo convertir el audio descargado. Consulta el registro."); return;
            }
            m_downloadFiles[m_downloadIndex++] = m_encodedDownload;
            convertDownloadedAudio();
            return;
        }
        if (success && j.kind != Source::File) {
            if (j.kind == Source::YouTube && isAudio(j.format)) {
                if (m_downloadFiles.isEmpty()) {
                    complete(false, "No se ha confirmado ningún archivo de audio descargado."); return;
                }
                convertDownloadedAudio();
                return;
            }
            if (j.kind == Source::Spotify) {
                QFile errors(m_spotifyErrors);
                if (errors.open(QIODevice::ReadOnly)) {
                    const auto detail = QString::fromUtf8(errors.readAll()).trimmed();
                    if (!detail.isEmpty()) {
                        emit log(detail);
                        complete(false, "Descarga con errores parciales. Consulta el registro."); return;
                    }
                }
                QFile metadata(m_spotifyMetadata);
                if (!metadata.open(QIODevice::ReadOnly)) {
                    complete(false, "spotDL no ha generado la relación de canciones. Consulta el registro."); return;
                }
                const auto document = QJsonDocument::fromJson(metadata.readAll());
                bool metadataValid = document.isArray() && !document.array().isEmpty();
                QSet<QString> ids;
                for (const auto &value : document.array()) {
                    const auto id = value.toObject().value("song_id").toString();
                    if (QRegularExpression("^[A-Za-z0-9]{22}$").match(id).hasMatch()) ids.insert(id);
                    else metadataValid = false;
                }
                const auto files = QDir(m_downloadTemp->path()).entryInfoList({"*." + j.format}, QDir::Files);
                for (const auto &id : ids) {
                    const QString prefix = id + " - ";
                    for (const auto &file : files) {
                        if (file.fileName().startsWith(prefix) && file.size() > 0) {
                            m_downloadFiles << file.absoluteFilePath();
                            break;
                        }
                    }
                }
                if (!metadataValid || ids.isEmpty() || m_downloadFiles.size() != ids.size()) {
                    complete(false, "Faltan archivos de las canciones solicitadas. Consulta el registro."); return;
                }
            }
            bool valid = !m_downloadFiles.isEmpty();
            for (const auto &path : m_downloadFiles) {
                const QFileInfo file(path);
                valid &= file.isFile() && file.size() > 0 && file.suffix().compare(j.format, Qt::CaseInsensitive) == 0;
            }
            if (!valid) {
                complete(false, "No se han confirmado archivos de esta descarga en el formato solicitado."); return;
            }
            QString error;
            if (!publishDownloads(&error)) { complete(false, error); return; }
        }
        if (success && j.kind == Source::File) {
            QFile staged(m_staged);
            success = staged.size() > 0 && staged.rename(m_output);
            if (success) j.result = m_output;
            else { complete(false, "No se pudo guardar la salida en " + QDir::toNativeSeparators(m_output)
                            + ": " + staged.errorString()); return; }
        }
        complete(success, success ? "Finalizado." : "La herramienta terminó con código "
                 + QString::number(code) + ". Consulta el registro.");
    });
    process->start(program, arguments);
    if (probe) QTimer::singleShot(15000, process, [process] {
        if (process->state() != QProcess::NotRunning) process->kill();
    });
}

void Runner::consume(bool final)
{
    if (!m_process) return;
    const auto bytes = m_process->readAllStandardOutput();
    if (m_probe) { m_probeData += bytes; return; }
    m_buffer += bytes;
    m_buffer.replace('\r', '\n');
    while (m_buffer.contains('\n') || (final && !m_buffer.isEmpty())) {
        int end = m_buffer.indexOf('\n');
        if (end < 0) end = m_buffer.size();
        const QString line = QString::fromUtf8(m_buffer.left(end)).trimmed();
        m_buffer.remove(0, end + 1);
        if (line.isEmpty()) continue;
        if (line.contains("Could not create temporary directory", Qt::CaseInsensitive))
            m_temporaryFailure = true;
        auto &j = m_model->jobs[m_row];
        if (line.startsWith("CONVERT_FILE:")) {
            const auto values = QJsonDocument::fromJson(("[" + line.mid(13) + "]").toUtf8()).array();
            if (!values.isEmpty() && values.first().isString()) {
                const auto path = values.first().toString();
                if (!m_downloadFiles.contains(path)) m_downloadFiles << path;
            }
        } else if (line.startsWith("out_time_us=") && m_duration > 0) {
            j.progress = qBound(0, int(line.mid(12).toDouble() / (m_duration * 10000.0)), 99);
            m_model->refresh(m_row);
        } else if (line.startsWith("CONVERT_PROGRESS:")) {
            QString number = line.mid(17);
            number.remove('%');
            bool ok = false;
            const double percent = number.trimmed().toDouble(&ok);
            if (ok) { j.progress = qBound(0, int(percent), 99); m_model->refresh(m_row); }
        } else if ((j.kind != Source::File && !m_encodingDownload) || !QRegularExpression("^[a-z_0-9]+=.*$").match(line).hasMatch()) {
            j.detail = line;
            emit log(line);
        }
    }
}

bool Runner::publishDownloads(QString *error)
{
    const auto &job = m_model->jobs[m_row];
    const QDir destination(job.destination);
    QStringList saved;
    for (const auto &path : m_downloadFiles) {
        QString name = QFileInfo(path).completeBaseName();
        if (job.kind == Source::Spotify)
            name.remove(QRegularExpression("^[A-Za-z0-9]{22} - "));
        else
            name.remove(QRegularExpression(" \\[[A-Za-z0-9_-]{11}\\]$"));
        name = name.trimmed();
        if (name.isEmpty()) name = "descarga";
        const QString extension = "." + job.format;
        QString target = destination.filePath(name + extension);
        int suffix = 2;
        while (QFileInfo::exists(target))
            target = destination.filePath(name + " (" + QString::number(suffix++) + ")" + extension);
        QFile file(path);
        if (!file.rename(target)) {
            *error = "No se pudo guardar " + QDir::toNativeSeparators(target) + ": " + file.errorString();
            return false;
        }
        saved << target;
        emit log("Guardado: " + QDir::toNativeSeparators(target));
    }
    m_downloadFiles = saved;
    return true;
}

void Runner::complete(bool success, const QString &message)
{
    if (m_row < 0 || m_row >= m_model->jobs.size()) return;
    auto &j = m_model->jobs[m_row];
    const bool completed = success && !m_cancelled;
    QString detail = message;
    if (!completed && m_downloadTemp && m_downloadTemp->isValid() &&
        !QDir(m_downloadTemp->path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        m_downloadTemp->setAutoRemove(false);
        detail += "\nDescarga parcial conservada en: " + QDir::toNativeSeparators(m_downloadTemp->path());
    }
    if (completed) {
        emit log("Completado: " + j.source);
        ++m_completedCount;
        const int row = m_row;
        m_row = -1;
        m_model->remove(row);
    } else {
        j.state = m_cancelled ? State::Cancelled : State::Failed;
        if (j.progress < 0) j.progress = 0;
        j.detail = detail;
        m_model->refresh(m_row);
    }
    emit checkpoint();
    emit log(detail);
    if (!m_staged.isEmpty()) {
        if (QFileInfo::exists(m_staged) && !QFile::remove(m_staged))
            emit log("No se pudo eliminar el archivo temporal: " + QDir::toNativeSeparators(m_staged));
        m_staged.clear();
    }
    m_downloadTemp.reset();
    m_runtimeTemp.reset();
    QTimer::singleShot(0, this, &Runner::next);
}
