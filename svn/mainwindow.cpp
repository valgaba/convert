#include "mainwindow.h"
#include "aboutdialog.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

MainWindow::MainWindow()
{
    setWindowTitle("Convert · Audio y vídeo");
    setObjectName("ConvertWindow");
    setWindowIcon(QIcon(":/convert.png"));
    resize(1180, 760);
    setMinimumSize(850, 580);
    setAcceptDrops(true);
    m_model = new JobModel(this);
    m_runner = new Runner(m_model, this);

    auto *fileMenu = menuBar()->addMenu("&Archivo");
    auto *addAction = fileMenu->addAction("Añadir archivos…");
    addAction->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("Salir", QKeySequence::Quit, this, &QWidget::close);
    m_preferences = menuBar()->addAction("Preferencias…", this, &MainWindow::preferences);
    auto *helpMenu = menuBar()->addMenu("Ayuda");
    helpMenu->addAction("Acerca de Convert", this, [this] {
        AboutDialog dialog(this);
        dialog.exec();
    });

    auto *central = new QWidget;
    central->setObjectName("ConvertCentral");
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);
    auto *brand = new QHBoxLayout;
    brand->setSpacing(12);
    auto *logo = new QLabel;
    logo->setPixmap(QIcon(":/convert.png").pixmap(QSize(48, 48), devicePixelRatioF()));
    logo->setFixedSize(48, 48);
    logo->setAccessibleName("Convert");
    brand->addWidget(logo);
    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(2);
    auto *title = new QLabel("convert");
    title->setObjectName("title");
    brandText->addWidget(title);
    brandText->addWidget(new QLabel("Conversión de audio y vídeo · Descargas"));
    brand->addLayout(brandText);
    brand->addStretch();
    layout->addLayout(brand);

    auto *sourceBox = new QFrame;
    sourceBox->setProperty("panel", true);
    auto *sourceLayout = new QVBoxLayout(sourceBox);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    sourceLayout->setSpacing(0);
    auto *sourceTitle = new QLabel("Archivos y enlaces");
    sourceTitle->setObjectName("PanelTitle");
    sourceLayout->addWidget(sourceTitle);
    auto *sourceContent = new QWidget;
    auto *sourceBody = new QVBoxLayout(sourceContent);
    sourceBody->setContentsMargins(10, 10, 10, 10);
    auto *sourceRow = new QHBoxLayout;
    auto *addButton = new QPushButton("Añadir archivos…");
    m_url = new QLineEdit;
    m_url->setPlaceholderText("Pega una URL de YouTube o Spotify…");
    m_url->setClearButtonEnabled(true);
    auto *paste = new QPushButton("Pegar");
    auto *addLink = new QPushButton("Añadir URL");
    sourceRow->addWidget(addButton);
    sourceRow->addWidget(m_url, 1);
    sourceRow->addWidget(paste);
    sourceRow->addWidget(addLink);
    sourceBody->addLayout(sourceRow);
    sourceBody->addWidget(new QLabel("Arrastra archivos o enlaces sobre la lista. Spotify: canciones, álbumes y listas públicas."));
    sourceLayout->addWidget(sourceContent);
    layout->addWidget(sourceBox);

    auto *outputBox = new QFrame;
    outputBox->setProperty("panel", true);
    auto *outputLayout = new QVBoxLayout(outputBox);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(0);
    auto *outputTitle = new QLabel("Formato y destino");
    outputTitle->setObjectName("PanelTitle");
    outputLayout->addWidget(outputTitle);
    auto *outputContent = new QWidget;
    auto *outputRow = new QHBoxLayout(outputContent);
    outputRow->setContentsMargins(10, 10, 10, 10);
    outputLayout->addWidget(outputContent);
    m_format = new QComboBox;
    m_format->addItems(formats());
    m_format->setCurrentText(QSettings().value("output/format", "mp3").toString());
    const auto savedDestination = QSettings().value("output/directory").toString();
    m_destination = new QLineEdit(savedDestination.trimmed().isEmpty()
        ? QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("salida"))
        : savedDestination);
    m_destination->setToolTip("Los archivos se guardan directamente aquí. Para cambiar entradas existentes, usa Aplicar a selección.");
    auto *browse = new QPushButton("Examinar…");
    auto *apply = new QPushButton("Aplicar a selección");
    outputRow->addWidget(new QLabel("Formato"));
    outputRow->addWidget(m_format);
    outputRow->addSpacing(12);
    outputRow->addWidget(new QLabel("Carpeta"));
    outputRow->addWidget(m_destination, 1);
    outputRow->addWidget(browse);
    outputRow->addWidget(apply);
    layout->addWidget(outputBox);

    auto *splitter = new QSplitter(Qt::Vertical);
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->setAcceptDrops(true);
    m_table->viewport()->setAcceptDrops(true);
    m_table->viewport()->installEventFilter(this);
    splitter->addWidget(m_table);
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1800);
    m_log->setPlaceholderText("Registro de actividad. Los errores de las herramientas aparecerán aquí.");
    splitter->addWidget(m_log);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({400, 100});
    layout->addWidget(splitter, 1);

    auto *actions = new QHBoxLayout;
    auto *remove = new QPushButton("Quitar selección");
    auto *clearDone = new QPushButton("Limpiar completados");
    auto *retry = new QPushButton("Reintentar selección");
    auto *open = new QPushButton("Abrir destino");
    m_start = new QPushButton("Iniciar cola");
    m_start->setObjectName("primary");
    m_stop = new QPushButton("Detener cola");
    m_stop->setEnabled(false);
    for (auto *button : {remove, clearDone, retry, open}) actions->addWidget(button);
    actions->addStretch();
    actions->addWidget(m_stop);
    actions->addWidget(m_start);
    layout->addLayout(actions);
    m_summary = new QLabel;
    m_progress = new QProgressBar;
    m_progress->setMaximumWidth(220);
    statusBar()->addWidget(m_summary, 1);
    statusBar()->addPermanentWidget(m_progress);
    setCentralWidget(central);
    m_controls = {sourceBox, outputBox, remove, clearDone, retry};

    const auto chooseFiles = [this] {
        addFiles(QFileDialog::getOpenFileNames(this, "Añadir audio o vídeo", {},
            "Audio y vídeo (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav *.flac *.m4a *.aac *.ogg *.opus *.wma *.wmv *.mpeg *.mpg *.m4v *.aiff *.aif *.ts);;Todos los archivos (*)"));
    };
    connect(addButton, &QPushButton::clicked, this, chooseFiles);
    connect(addAction, &QAction::triggered, this, chooseFiles);
    connect(m_runner, &Runner::busyChanged, addAction, [addAction](bool busy) { addAction->setEnabled(!busy); });
    connect(addLink, &QPushButton::clicked, this, &MainWindow::addFromField);
    connect(m_url, &QLineEdit::returnPressed, this, &MainWindow::addFromField);
    connect(paste, &QPushButton::clicked, this, [this] { m_url->setText(QApplication::clipboard()->text().trimmed()); });
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto dir = QFileDialog::getExistingDirectory(this, "Carpeta de salida", m_destination->text());
        if (!dir.isEmpty()) m_destination->setText(dir);
    });
    connect(apply, &QPushButton::clicked, this, [this] {
        const auto rows = selectedRows();
        for (int row : rows) {
            const auto &j = m_model->jobs[row];
            if (j.state == State::Done) continue;
            if (j.kind == Source::Spotify && !isAudio(m_format->currentText())) {
                QMessageBox::information(this, "Formato de Spotify", "Selecciona un formato de audio para las entradas de Spotify.");
                return;
            }
        }
        const auto destination = outputDirectory(m_destination->text());
        if (destination.isEmpty()) {
            QMessageBox::information(this, "Destino", "Selecciona una carpeta de salida."); return;
        }
        for (int row : rows) {
            auto &j = m_model->jobs[row];
            if (j.state == State::Done) continue;
            j.format = m_format->currentText();
            j.destination = destination;
            j.state = State::Pending; j.progress = 0; j.result.clear(); j.detail.clear();
            m_model->refresh(row);
        }
        saveQueue();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        auto rows = selectedRows();
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int row : rows) m_model->remove(row);
        saveQueue();
    });
    connect(clearDone, &QPushButton::clicked, this, [this] {
        for (int row = m_model->jobs.size() - 1; row >= 0; --row)
            if (m_model->jobs[row].state == State::Done) m_model->remove(row);
        saveQueue();
    });
    connect(retry, &QPushButton::clicked, this, [this] {
        for (int row : selectedRows()) {
            auto &j = m_model->jobs[row];
            if (j.state != State::Failed && j.state != State::Cancelled) continue;
            j.state = State::Pending; j.progress = 0; j.detail.clear(); j.result.clear();
            m_model->refresh(row);
        }
        saveQueue();
    });
    connect(open, &QPushButton::clicked, this, [this] {
        auto path = m_destination->text();
        const auto rows = selectedRows();
        if (!rows.isEmpty()) {
            const auto &j = m_model->jobs[rows.first()];
            path = j.result.isEmpty() ? j.destination :
                j.kind == Source::File ? QFileInfo(j.result).absolutePath() : j.result;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect(m_start, &QPushButton::clicked, m_runner, &Runner::start);
    connect(m_stop, &QPushButton::clicked, m_runner, &Runner::cancel);
    connect(m_runner, &Runner::busyChanged, this, &MainWindow::setBusy);
    connect(m_runner, &Runner::checkpoint, this, &MainWindow::saveQueue);
    connect(m_runner, &Runner::log, m_log, &QPlainTextEdit::appendPlainText);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &MainWindow::updateSummary);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateSummary);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateSummary);
    restoreQueue();
    restoreGeometry(QSettings().value("window/geometry").toByteArray());
    updateSummary();
    m_log->appendPlainText("Preparado. Configura FFmpeg, ffprobe, yt-dlp y spotDL en Preferencias.");
}

QList<int> MainWindow::selectedRows() const
{
    QList<int> rows;
    for (const auto &index : m_table->selectionModel()->selectedRows()) rows << index.row();
    return rows;
}

void MainWindow::addFiles(const QStringList &files)
{
    if (m_runner->busy()) return;
    const auto dest = outputDirectory(m_destination->text());
    if (dest.isEmpty()) { QMessageBox::information(this, "Destino", "Selecciona una carpeta de salida."); return; }
    for (const auto &file : files) {
        const QFileInfo info(file);
        if (!info.isFile()) { m_log->appendPlainText("Omitido (no es un archivo): " + file); continue; }
        Job j;
        j.source = info.absoluteFilePath(); j.format = m_format->currentText();
        j.destination = dest;
        m_model->append(j);
    }
    saveQueue();
}

bool MainWindow::addUrl(const QString &text, QString *error)
{
    Source source;
    if (!classifyUrl(text, &source)) { *error = "URL no admitida: " + text; return false; }
    if (source == Source::Spotify && !isAudio(m_format->currentText())) {
        *error = "Spotify requiere MP3, WAV, FLAC, M4A, OPUS u OGG."; return false;
    }
    const auto destination = outputDirectory(m_destination->text());
    if (destination.isEmpty()) { *error = "Selecciona una carpeta de salida."; return false; }
    Job j;
    j.source = text; j.kind = source; j.format = m_format->currentText();
    j.destination = destination;
    m_model->append(j);
    return true;
}

void MainWindow::addFromField()
{
    if (m_runner->busy()) return;
    const auto urls = m_url->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QStringList errors;
    for (const auto &url : urls) { QString error; if (!addUrl(url, &error)) errors << error; }
    if (errors.isEmpty()) m_url->clear();
    else QMessageBox::information(this, "Revisar enlaces", errors.join('\n'));
    saveQueue();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!m_runner->busy() && (event->mimeData()->hasUrls() || event->mimeData()->hasText()))
        event->acceptProposedAction();
}

void MainWindow::handleDrop(const QMimeData *data)
{
    if (m_runner->busy()) return;
    QStringList files, links, errors;
    if (data->hasUrls()) {
        for (const auto &url : data->urls()) {
            if (url.isLocalFile()) files << url.toLocalFile();
            else links << url.toString();
        }
    } else links = data->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (!files.isEmpty()) addFiles(files);
    for (const auto &link : links) { QString error; if (!addUrl(link, &error)) errors << error; }
    if (!errors.isEmpty()) QMessageBox::information(this, "Elementos omitidos", errors.join('\n'));
    saveQueue();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (m_runner->busy()) return;
    handleDrop(event->mimeData()); event->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_table->viewport()) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *drag = static_cast<QDragMoveEvent *>(event);
            if (!m_runner->busy() && (drag->mimeData()->hasUrls() || drag->mimeData()->hasText()))
                drag->acceptProposedAction();
            else drag->ignore();
            return true;
        }
        if (event->type() == QEvent::Drop) { dropEvent(static_cast<QDropEvent *>(event)); return true; }
    }
    return QMainWindow::eventFilter(object, event);
}

void MainWindow::setBusy(bool busy)
{
    for (auto *control : m_controls) control->setEnabled(!busy);
    m_preferences->setEnabled(!busy);
    m_stop->setEnabled(busy);
    saveQueue();
    updateSummary();
    if (!busy && m_closeAfterCancel) QTimer::singleShot(0, this, &QWidget::close);
}

void MainWindow::updateSummary()
{
    int done = 0, pending = 0, failed = 0;
    int current = 0;
    for (const auto &j : m_model->jobs) {
        done += j.state == State::Done;
        pending += j.state == State::Pending;
        failed += j.state == State::Failed;
        if (j.state == State::Running) current = j.progress;
    }
    m_summary->setText(QString("%1 entradas  ·  %2 pendientes  ·  %3 completadas  ·  %4 errores")
        .arg(m_model->jobs.size()).arg(pending).arg(done).arg(failed));
    m_start->setEnabled(!m_runner->busy() && pending > 0);
    m_progress->setRange(0, current < 0 ? 0 : 100);
    m_progress->setValue(m_runner->busy() ? qMax(0, current) : (done > 0 && pending == 0 ? 100 : 0));
}

void MainWindow::preferences()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Preferencias · Herramientas");
    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel("Selecciona ejecutables o deja el campo vacío para buscar primero en la carpeta bin junto a Convert.\n"
                            "Después se busca en ffmpeg, tools, junto al programa y en PATH.\n"
                            "FFmpeg es obligatorio. ffprobe permite calcular el progreso de conversión.");
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *form = new QFormLayout;
    QMap<QString, QLineEdit *> entries;
    for (const auto &name : QStringList{"ffmpeg", "ffprobe", "yt-dlp", "spotdl"}) {
        auto *row = new QHBoxLayout;
        auto *edit = new QLineEdit(QSettings().value("tools/" + name).toString());
        edit->setPlaceholderText(toolPath(name).isEmpty() ? "No encontrado" : toolPath(name));
        edit->setMinimumWidth(400);
        auto *browse = new QPushButton("Examinar…");
        row->addWidget(edit); row->addWidget(browse);
        form->addRow(name, row); entries.insert(name, edit);
        connect(browse, &QPushButton::clicked, &dialog, [&dialog, edit] {
            const auto path = QFileDialog::getOpenFileName(&dialog, "Seleccionar ejecutable", edit->text());
            if (!path.isEmpty()) edit->setText(path);
        });
    }
    layout->addLayout(form);
    auto *info = new QLabel("Spotify utiliza spotDL como herramienta externa; el audio procede de YouTube.\n"
                           "Las descargas pueden depender de disponibilidad, autenticación y cambios del servicio.");
    info->setWordWrap(true); layout->addWidget(info);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText("Guardar");
    buttons->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, entries] {
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            const auto path = it.value()->text().trimmed();
            if (!path.isEmpty() && (!QFileInfo(path).isFile() || !QFileInfo(path).isExecutable())) {
                QMessageBox::warning(&dialog, "Ruta incorrecta", "Revisa el ejecutable de " + it.key()); return;
            }
        }
        QSettings settings;
        for (auto it = entries.cbegin(); it != entries.cend(); ++it)
            settings.setValue("tools/" + it.key(), it.value()->text().trimmed());
        dialog.accept();
    });
    dialog.exec();
}

void MainWindow::saveQueue()
{
    QSettings s;
    s.setValue("output/format", m_format->currentText());
    s.setValue("output/directory", m_destination->text());
    s.beginWriteArray("queue", m_model->jobs.size());
    for (int row = 0; row < m_model->jobs.size(); ++row) {
        const auto &j = m_model->jobs[row];
        s.setArrayIndex(row);
        s.setValue("source", j.source); s.setValue("kind", int(j.kind));
        s.setValue("format", j.format); s.setValue("destination", j.destination);
        s.setValue("state", int(j.state)); s.setValue("result", j.result);
        s.setValue("detail", j.detail);
    }
    s.endArray();
}

void MainWindow::restoreQueue()
{
    QSettings s;
    const int size = s.beginReadArray("queue");
    for (int row = 0; row < size; ++row) {
        s.setArrayIndex(row);
        Job j;
        j.source = s.value("source").toString();
        const int kind = s.value("kind").toInt();
        if (kind < 0 || kind > 2) continue;
        j.kind = Source(kind); j.format = s.value("format").toString();
        j.destination = s.value("destination").toString();
        if (!formats(j.kind == Source::Spotify).contains(j.format) || j.destination.isEmpty()) continue;
        Source parsed;
        if (j.kind != Source::File && (!classifyUrl(j.source, &parsed) || parsed != j.kind)) continue;
        const int state = s.value("state").toInt();
        j.state = state >= 0 && state <= 4 ? State(state) : State::Pending;
        if (j.state == State::Running) j.state = State::Cancelled;
        j.result = s.value("result").toString(); j.detail = s.value("detail").toString();
        j.progress = j.state == State::Done ? 100 : 0;
        m_model->append(j);
    }
    s.endArray();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_runner->busy()) {
        const auto answer = QMessageBox::question(this, "Trabajo en curso",
            "¿Detener la cola y cerrar? Las descargas parciales permanecerán en su carpeta.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        event->ignore();
        if (answer == QMessageBox::Yes) { m_closeAfterCancel = true; m_runner->cancel(); }
        return;
    }
    saveQueue();
    QSettings().setValue("window/geometry", saveGeometry());
    event->accept();
}
