#include "updates.h"
#include "jobs.h"
#include <QCheckBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>
#include <QVersionNumber>
#include <QVBoxLayout>
#include <memory>
#include <utility>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#endif

namespace {
QString numericVersion(const QString &text)
{
    return QRegularExpression("\\b[vn]?(\\d+(?:\\.\\d+){1,3})").match(text).captured(1);
}

QString cacheRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath("updates");
}

QNetworkRequest requestFor(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "RaditTools-Convert-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    return request;
}

QString powershell()
{
    return QDir(qEnvironmentVariable("SystemRoot", "C:/Windows"))
        .filePath("System32/WindowsPowerShell/v1.0/powershell.exe");
}

void hideConsole(QProcess *process)
{
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
        arguments->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        arguments->startupInfo->wShowWindow = SW_HIDE;
    });
#else
    Q_UNUSED(process);
#endif
}

bool writeJson(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    const auto data = QJsonDocument(object).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
}

UpdateManager::UpdateManager(std::function<bool()> queueBusy, QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)), m_queueBusy(std::move(queueBusy))
{
    const auto add = [this](const QString &key, const QString &name, const QString &repository) {
        UpdateItem item;
        item.key = key; item.name = name; item.repository = repository;
        item.status = QStringLiteral("Sin comprobar");
        m_items << item;
    };
    add("convert", "Convert", "valgaba/convert");
    add("ffmpeg", "FFmpeg / ffprobe / ffplay", "GyanD/codexffmpeg");
    add("yt-dlp", "yt-dlp", "yt-dlp/yt-dlp");
    add("spotdl", "spotDL", "spotDL/spotify-downloader");
    add("deno", "Deno", "denoland/deno");
}

UpdateManager::~UpdateManager()
{
    if (m_download) { m_download->disconnect(this); m_download->abort(); }
    for (auto *process : findChildren<QProcess *>()) { process->disconnect(this); process->kill(); }
    delete m_package;
    delete m_hash;
}

void UpdateManager::checkAutomatically()
{
    QSettings settings;
    const auto pending = settings.value("updates/pendingResult").toString();
    if (!pending.isEmpty()) {
        const auto path = QDir::cleanPath(QFileInfo(pending).absoluteFilePath());
        const auto root = QDir::cleanPath(QFileInfo(cacheRoot()).absoluteFilePath());
        if (path.startsWith(root + '/', Qt::CaseInsensitive) && QFileInfo(path).fileName() == "result.json") {
            QFile result(path);
            if (result.open(QIODevice::ReadOnly)) {
                const auto state = QJsonDocument::fromJson(result.readAll()).object();
                const bool failed = state.value("status").toString() != "ok";
                m_message = failed ? QStringLiteral("No se pudo actualizar Convert: ") + state.value("error").toString()
                                   : QStringLiteral("Convert se ha actualizado a la versión %1.").arg(QCoreApplication::applicationVersion());
                emit log(m_message); emit installationReport(m_message, failed); emit changed();
                settings.remove("updates/pendingResult");
            }
        } else settings.remove("updates/pendingResult");
    }
    if (!settings.value("updates/checkAtStartup", true).toBool()) return;
    const auto last = settings.value("updates/lastCheck").toDateTime();
    if (!last.isValid() || last.secsTo(QDateTime::currentDateTimeUtc()) >= 86400) check();
}

void UpdateManager::check()
{
    if (m_busy) return;
    m_busy = true;
    m_progress = -1;
    m_message = QStringLiteral("Comprobando versiones instaladas y publicaciones…");
    QSettings().setValue("updates/lastCheck", QDateTime::currentDateTimeUtc());
    m_pending = int(m_items.size()) * 2;
    for (int row = 0; row < m_items.size(); ++row) {
        auto &item = m_items[row];
        item.installed.clear(); item.latest.clear(); item.digest.clear(); item.assetName.clear();
        item.download.clear(); item.release.clear(); item.localError.clear(); item.remoteError.clear();
        item.missing = item.available = false;
        item.status = QStringLiteral("Comprobando…");
        probe(row);
        fetchRelease(row);
    }
    emit changed();
}

void UpdateManager::probe(int row)
{
    auto &item = m_items[row];
    if (item.key == "convert") {
        item.installed = QCoreApplication::applicationVersion();
        checkedOne(); return;
    }
    const QString program = toolPath(item.key);
    if (program.isEmpty() || !QFileInfo(program).isFile()) {
        item.installed = QStringLiteral("No instalado"); item.missing = true;
        checkedOne(); return;
    }
    const QString temporary = QDir(cacheRoot()).filePath("probe-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(temporary)) {
        item.localError = QStringLiteral("No se puede preparar la consulta local"); checkedOne(); return;
    }
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("TEMP", QDir::toNativeSeparators(temporary));
    environment.insert("TMP", QDir::toNativeSeparators(temporary));
    environment.insert("TMPDIR", QDir::toNativeSeparators(temporary));
    environment.insert("PYTHONIOENCODING", "utf-8");
    if (item.key == "spotdl") {
        const auto profile = QDir(cacheRoot()).filePath("spotdl-profile");
        QDir().mkpath(QDir(profile).filePath(".spotdl"));
        environment.insert("USERPROFILE", QDir::toNativeSeparators(profile));
    }
    auto *process = new QProcess(this);
    hideConsole(process);
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::MergedChannels);
    auto output = std::make_shared<QByteArray>();
    auto done = std::make_shared<bool>(false);
    const auto finish = [this, process, output, done, row, temporary](bool success) {
        if (*done) return;
        *done = true;
        output->append(process->readAllStandardOutput());
        auto &entry = m_items[row];
        const auto version = success ? numericVersion(QString::fromUtf8(*output)) : QString();
        if (version.isEmpty()) entry.localError = QStringLiteral("No se pudo leer la versión instalada");
        else {
            entry.installed = version;
            if (entry.key == "ffmpeg") {
                for (const auto &name : QStringList{"ffmpeg", "ffprobe", "ffplay"})
                    entry.missing |= !QFileInfo(toolPath(name)).isFile();
            }
        }
        // Only this unique probe directory is removed; no application data.
        const auto probePath = QFileInfo(temporary).canonicalFilePath();
        const auto cachePath = QDir(cacheRoot()).canonicalPath();
        if (!probePath.isEmpty() && !cachePath.isEmpty() && !QFileInfo(temporary).isSymLink()
            && probePath.startsWith(cachePath + "/probe-", Qt::CaseInsensitive))
            QDir(probePath).removeRecursively();
        process->deleteLater();
        checkedOne();
    };
    connect(process, &QProcess::readyReadStandardOutput, this, [process, output] {
        output->append(process->readAllStandardOutput());
        if (output->size() > 65536) process->kill();
    });
    connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finish(false);
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [finish](int code, QProcess::ExitStatus status) { finish(code == 0 && status == QProcess::NormalExit); });
    process->start(program, {item.key == "ffmpeg" ? "-version" : "--version"});
    QTimer::singleShot(15000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
}

void UpdateManager::fetchRelease(int row)
{
    const auto &item = m_items[row];
    const auto url = QUrl("https://api.github.com/repos/" + item.repository
                         + "/releases?per_page=" + (item.key == "ffmpeg" ? "100" : "10"));
    auto *reply = m_network->get(requestFor(url));
    auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, bytes] {
        bytes->append(reply->readAll());
        if (bytes->size() > 16 * 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, row] {
        bytes->append(reply->readAll());
        auto &entry = m_items[row];
        const auto document = QJsonDocument::fromJson(*bytes);
        if (reply->error() != QNetworkReply::NoError || !document.isArray()) {
            entry.remoteError = QStringLiteral("No se pudo consultar GitHub (%1)")
                .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        } else {
            const bool previews = entry.key == "convert" && QSettings().value("updates/prereleases", false).toBool();
            for (const auto &value : document.array()) {
                const auto release = value.toObject();
                if (release.value("draft").toBool() || (release.value("prerelease").toBool() && !previews)) continue;
                const QString tag = release.value("tag_name").toString();
                if (entry.key == "ffmpeg" && !QRegularExpression("^\\d+\\.\\d+(?:\\.\\d+)?$").match(tag).hasMatch()) continue;
                const auto version = numericVersion(tag);
                if (version.isEmpty() || (!entry.latest.isEmpty() &&
                    QVersionNumber::compare(QVersionNumber::fromString(version), QVersionNumber::fromString(entry.latest)) <= 0)) continue;
                for (const auto &assetValue : release.value("assets").toArray()) {
                    const auto asset = assetValue.toObject();
                    const QString name = asset.value("name").toString();
                    const bool matches = entry.key == "convert" ? name.startsWith("convert-" + version + "-") && QRegularExpression("^convert-[0-9][A-Za-z0-9._-]*-windows-x64\\.zip$").match(name).hasMatch()
                        : entry.key == "ffmpeg" ? name == "ffmpeg-" + tag + "-essentials_build.zip"
                        : entry.key == "deno" ? name == "deno-x86_64-pc-windows-msvc.zip"
                        : entry.key == "yt-dlp" ? name == "yt-dlp.exe"
                        : name == "spotdl-" + version + "-win32.exe";
                    if (!matches) continue;
                    const QUrl download(asset.value("browser_download_url").toString());
                    const QString digest = asset.value("digest").toString();
                    if (download.scheme() != "https" || download.host() != "github.com" ||
                        !download.path().startsWith("/" + entry.repository + "/releases/download/")) continue;
                    entry.latest = version; entry.assetName = name; entry.download = download;
                    entry.release = QUrl(release.value("html_url").toString());
                    entry.size = qint64(asset.value("size").toDouble());
                    entry.digest = QRegularExpression("^sha256:[a-fA-F0-9]{64}$").match(digest).hasMatch() ? digest.mid(7).toLower() : QString();
                    break;
                }
            }
        }
        reply->deleteLater();
        checkedOne();
    });
}

void UpdateManager::checkedOne()
{
    if (--m_pending > 0) return;
    int count = 0;
    for (auto &item : m_items) {
        if (!item.localError.isEmpty()) item.status = item.localError;
        else if (!item.remoteError.isEmpty()) item.status = item.remoteError;
        else if (item.latest.isEmpty()) item.status = QStringLiteral("Sin publicación compatible en este canal");
        else if (item.digest.isEmpty()) item.status = QStringLiteral("La publicación no incluye SHA-256");
        else if ((item.missing && (numericVersion(item.installed).isEmpty() ||
                 QVersionNumber::compare(QVersionNumber::fromString(item.latest), QVersionNumber::fromString(item.installed)) >= 0)) ||
                 QVersionNumber::compare(QVersionNumber::fromString(item.latest), QVersionNumber::fromString(item.installed)) > 0) {
            item.available = true; ++count;
            item.status = item.missing ? QStringLiteral("Disponible para instalar") : QStringLiteral("Nueva versión disponible");
        } else item.status = QStringLiteral("Al día");
    }
    m_busy = false;
    m_message = count ? QStringLiteral("%1 actualizaciones disponibles. Elige un componente para actualizar.").arg(count)
                      : QStringLiteral("Comprobación terminada. Consulta el estado de cada componente.");
    emit changed();
    emit available(count);
}

void UpdateManager::install(int row)
{
    if (m_busy || row < 0 || row >= m_items.size() || !m_items[row].available) return;
    if (m_queueBusy()) { m_message = QStringLiteral("Detén la cola antes de actualizar."); emit changed(); return; }
#ifndef Q_OS_WIN
    m_message = QStringLiteral("La instalación integrada está disponible para Windows x64."); emit changed(); return;
#endif
    QTemporaryFile writable(QDir(QCoreApplication::applicationDirPath()).filePath(".update-write-XXXXXX"));
    if (!writable.open()) { m_message = QStringLiteral("La carpeta de Convert no permite escritura: ") + writable.errorString(); emit changed(); return; }
    m_work = QDir(cacheRoot()).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(m_work)) { m_message = QStringLiteral("No se puede preparar la descarga."); emit changed(); return; }
    const auto item = m_items[row];
    if (item.size <= 0 || item.size > 2LL * 1024 * 1024 * 1024) {
        m_message = QStringLiteral("El tamaño publicado no es válido."); emit changed(); return;
    }
    delete m_package; delete m_hash;
    m_package = new QFile(QDir(m_work).filePath(item.assetName));
    m_hash = new QCryptographicHash(QCryptographicHash::Sha256);
    if (!m_package->open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        m_message = m_package->errorString(); emit changed(); return;
    }
    m_busy = m_installing = true; m_cancelled = false; m_writeError.clear(); m_progress = 0;
    emit installationChanged(true);
    m_message = QStringLiteral("Descargando %1 %2…").arg(item.name, item.latest);
    emit log(m_message);
    emit changed();
    auto request = requestFor(item.download);
    request.setRawHeader("Accept", "application/octet-stream");
    auto *reply = m_network->get(request);
    m_download = reply;
    const auto drain = [this, reply, item] {
        const auto bytes = reply->readAll();
        if (m_package->size() + bytes.size() > item.size || m_package->write(bytes) != bytes.size()) {
            m_writeError = QStringLiteral("Error al escribir la descarga o tamaño inesperado."); reply->abort(); return;
        }
        m_hash->addData(bytes);
    };
    connect(reply, &QNetworkReply::readyRead, this, drain);
    connect(reply, &QNetworkReply::downloadProgress, this, [this, item](qint64 received, qint64) {
        m_progress = qBound(0, int(100.0 * received / item.size), 100); emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, row, item, drain] {
        drain();
        const bool flushed = m_package->flush();
        m_package->close(); m_download = nullptr;
        const QString error = m_cancelled ? QStringLiteral("Actualización cancelada.")
            : !m_writeError.isEmpty() ? m_writeError
            : reply->error() != QNetworkReply::NoError ? reply->errorString()
            : !flushed || QFileInfo(m_package->fileName()).size() != item.size ? QStringLiteral("La descarga está incompleta.")
            : QString::fromLatin1(m_hash->result().toHex()) != item.digest ? QStringLiteral("La descarga no coincide con su SHA-256.") : QString();
        reply->deleteLater();
        if (!error.isEmpty()) { m_package->remove(); finishInstallation(error); return; }
        if (m_queueBusy()) { finishInstallation(QStringLiteral("La cola está en uso; no se han sustituido archivos.")); return; }
        beginInstaller(row);
    });
}

void UpdateManager::cancel()
{
    if (m_download) { m_cancelled = true; m_download->abort(); }
    else if (m_waitingRestart) {
        m_cancelled = true;
        QFile cancel(QDir(m_work).filePath("cancel"));
        if (cancel.open(QIODevice::WriteOnly)) cancel.write("cancel");
        m_message = QStringLiteral("Cancelando la preparación de la actualización…");
        emit changed();
    }
}

void UpdateManager::beginInstaller(int row)
{
    const auto item = m_items[row];
    const QDir work(m_work);
    QFile source(":/update-install.ps1");
    QFile script(work.filePath("update-install.ps1"));
    if (!source.open(QIODevice::ReadOnly) || !script.open(QIODevice::WriteOnly)) {
        finishInstallation(QStringLiteral("No se puede preparar el instalador.")); return;
    }
    const auto content = source.readAll();
    if (script.write(content) != content.size()) { finishInstallation(script.errorString()); return; }
    script.close();
    const auto plan = work.filePath("plan.json");
    if (!writeJson(plan, {{"schema", 1}, {"component", item.key}, {"version", item.latest},
                         {"sha256", item.digest}, {"package", m_package->fileName()},
                         {"application", QCoreApplication::applicationDirPath()},
                         {"parentPid", double(QCoreApplication::applicationPid())}})) {
        finishInstallation(QStringLiteral("No se puede guardar el plan de actualización.")); return;
    }
    m_progress = -1;
    m_message = QStringLiteral("Verificando e instalando %1…").arg(item.name);
    emit changed();
    const QStringList arguments{"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", script.fileName(), "-PlanPath", plan};
    if (item.key == "convert") {
        QProcess helper;
        hideConsole(&helper);
        helper.setProgram(powershell()); helper.setArguments(arguments); helper.setWorkingDirectory(m_work);
        if (!helper.startDetached()) { finishInstallation(QStringLiteral("No se pudo iniciar el instalador.")); return; }
        m_waitingRestart = true;
        emit changed();
        auto *timer = new QTimer(this);
        auto elapsed = std::make_shared<int>(0);
        connect(timer, &QTimer::timeout, this, [this, timer, work, elapsed] {
            *elapsed += 250;
            QFile result(work.filePath("result.json"));
            if (result.open(QIODevice::ReadOnly)) {
                timer->stop(); timer->deleteLater();
                const auto error = QJsonDocument::fromJson(result.readAll()).object().value("error").toString();
                finishInstallation(error.isEmpty() ? QStringLiteral("No se pudo preparar la actualización.") : error);
            } else if (!m_cancelled && QFileInfo::exists(work.filePath("ready"))) {
                timer->stop(); timer->deleteLater();
                m_waitingRestart = false;
                m_message = QStringLiteral("Reiniciando Convert para instalar la actualización…");
                QSettings().setValue("updates/pendingResult", work.filePath("result.json"));
                emit changed(); emit restartReady();
            } else if (*elapsed >= 240000) {
                QFile cancel(work.filePath("cancel")); if (cancel.open(QIODevice::WriteOnly)) cancel.write("cancel");
                timer->stop(); timer->deleteLater();
                finishInstallation(QStringLiteral("El instalador no pudo preparar la actualización. Convert sigue abierto."));
            }
        });
        timer->start(250);
        return;
    }
    auto *process = new QProcess(this);
    hideConsole(process);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setWorkingDirectory(m_work);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        const auto text = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        if (!text.isEmpty()) emit log(text);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { process->deleteLater(); finishInstallation(process->errorString()); }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, work, item](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        QFile result(work.filePath("result.json"));
        QJsonObject state;
        if (result.open(QIODevice::ReadOnly)) state = QJsonDocument::fromJson(result.readAll()).object();
        if (code != 0 || status != QProcess::NormalExit || state.value("status").toString() != "ok") {
            finishInstallation(state.value("error").toString(QStringLiteral("El instalador ha fallado. Consulta el registro."))); return;
        }
        const QDir tools(QDir(QCoreApplication::applicationDirPath()).filePath("tools"));
        QSettings settings;
        if (item.key == "ffmpeg") {
            for (const auto &name : QStringList{"ffmpeg", "ffprobe", "ffplay"}) settings.setValue("tools/" + name, tools.filePath(name + ".exe"));
        } else settings.setValue("tools/" + item.key, tools.filePath(item.key + ".exe"));
        finishInstallation({});
        check();
    });
    process->start(powershell(), arguments);
}

void UpdateManager::finishInstallation(const QString &error)
{
    m_busy = m_installing = m_waitingRestart = false; m_progress = error.isEmpty() ? 100 : 0;
    m_message = error.isEmpty() ? QStringLiteral("Actualización instalada. La copia anterior queda en .update-backup.") : error;
    emit log(m_message); emit installationChanged(false); emit changed();
}

UpdateDialog::UpdateDialog(UpdateManager *manager, QWidget *parent) : QDialog(parent), m_manager(manager)
{
    setWindowTitle(QStringLiteral("Actualizaciones · Convert"));
    resize(860, 440);
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(QStringLiteral("Compara las versiones instaladas con las publicaciones de sus proyectos.\n"
        "Las herramientas se instalan en tools; se conservan las rutas externas y se selecciona la copia nueva.\n"
        "Convert requiere reiniciarse. Tus preferencias, cola pendiente y carpetas de salida se conservan."));
    intro->setWordWrap(true); layout->addWidget(intro);
    m_table = new QTableWidget(int(manager->items().size()), 4);
    m_table->setHorizontalHeaderLabels({"Componente", "Instalada", "Disponible", "Estado"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    layout->addWidget(m_table, 1);
    auto *automatic = new QCheckBox(QStringLiteral("Comprobar al iniciar (como máximo una vez al día)"));
    automatic->setChecked(QSettings().value("updates/checkAtStartup", true).toBool());
    connect(automatic, &QCheckBox::toggled, this, [](bool enabled) { QSettings().setValue("updates/checkAtStartup", enabled); });
    layout->addWidget(automatic);
    auto *previews = new QCheckBox(QStringLiteral("Incluir versiones preliminares de Convert"));
    previews->setChecked(QSettings().value("updates/prereleases", false).toBool());
    connect(previews, &QCheckBox::toggled, this, [](bool enabled) { QSettings().setValue("updates/prereleases", enabled); });
    layout->addWidget(previews);
    m_message = new QLabel; m_message->setWordWrap(true); layout->addWidget(m_message);
    m_progress = new QProgressBar; layout->addWidget(m_progress);
    auto *buttons = new QHBoxLayout;
    m_check = new QPushButton("Comprobar ahora");
    m_release = new QPushButton("Ver publicación");
    m_install = new QPushButton("Actualizar seleccionado");
    m_cancel = new QPushButton("Cancelar descarga");
    auto *close = new QPushButton("Cerrar");
    for (auto *button : {m_check, m_release, m_install, m_cancel, close}) buttons->addWidget(button);
    layout->addLayout(buttons);
    connect(m_check, &QPushButton::clicked, manager, &UpdateManager::check);
    connect(m_cancel, &QPushButton::clicked, manager, &UpdateManager::cancel);
    connect(close, &QPushButton::clicked, this, &UpdateDialog::reject);
    connect(m_install, &QPushButton::clicked, this, [this] { m_manager->install(m_table->currentRow()); });
    connect(m_release, &QPushButton::clicked, this, [this] {
        const int row = m_table->currentRow();
        if (row >= 0) QDesktopServices::openUrl(m_manager->items()[row].release);
    });
    connect(manager, &UpdateManager::changed, this, &UpdateDialog::refresh);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &UpdateDialog::refresh);
    m_table->selectRow(0);
    refresh();
}

void UpdateDialog::refresh()
{
    const QSignalBlocker blocker(m_table);
    for (int row = 0; row < m_manager->items().size(); ++row) {
        const auto &item = m_manager->items()[row];
        const QStringList values{item.name, item.installed, item.latest, item.status};
        for (int column = 0; column < values.size(); ++column) {
            if (!m_table->item(row, column)) m_table->setItem(row, column, new QTableWidgetItem);
            m_table->item(row, column)->setText(values[column]);
            m_table->item(row, column)->setToolTip(values[column]);
        }
    }
    const int row = m_table->currentRow();
    m_check->setEnabled(!m_manager->busy());
    m_install->setEnabled(!m_manager->busy() && row >= 0 && m_manager->items()[row].available);
    m_install->setText(row == 0 ? "Actualizar y reiniciar" : "Actualizar herramienta");
    m_release->setEnabled(row >= 0 && !m_manager->items()[row].release.isEmpty());
    m_cancel->setEnabled(m_manager->canCancel());
    m_message->setText(m_manager->message());
    m_progress->setRange(0, m_manager->busy() && m_manager->progress() < 0 ? 0 : 100);
    m_progress->setValue(qMax(0, m_manager->progress()));
}

void UpdateDialog::reject()
{
    if (m_manager->installing()) { m_manager->cancel(); return; }
    QDialog::reject();
}
