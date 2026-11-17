#include "i18n.h"
#include "mainwindow.h"
#include "aboutdialog.h"
#include "updates.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
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
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
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
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace {
AudioOptions readAudio(QSettings &settings, const QString &prefix)
{
    AudioOptions options;
    options.bitrate = settings.value(prefix + "bitrate", 192).toInt();
    options.sampleRate = settings.value(prefix + "sampleRate", 0).toInt();
    options.channels = settings.value(prefix + "channels", 0).toInt();
    options.bitDepth = settings.value(prefix + "bitDepth", 16).toInt();
    return normalizedAudio(QString(), options);
}

void writeAudio(QSettings &settings, const QString &prefix, const AudioOptions &options)
{
    settings.setValue(prefix + "bitrate", options.bitrate);
    settings.setValue(prefix + "sampleRate", options.sampleRate);
    settings.setValue(prefix + "channels", options.channels);
    settings.setValue(prefix + "bitDepth", options.bitDepth);
}

bool editAudio(QWidget *parent, const QString &format, AudioOptions *options)
{
    const auto current = normalizedAudio(format, *options);
    QDialog dialog(parent);
    dialog.setWindowTitle(i18n::text("Opciones de audio · ") + format.toUpper());
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *bitrate = new QComboBox;
    for (int value : {64, 96, 128, 160, 192, 224, 256, 320})
        bitrate->addItem(QString::number(value) + " kb/s", value);
    bitrate->setCurrentIndex(bitrate->findData(current.bitrate));
    bitrate->setEnabled(format != "wav" && format != "flac");
    form->addRow(i18n::text("Tasa de bits"), bitrate);
    auto *rate = new QComboBox;
    if (format != "opus") rate->addItem(i18n::text("Automática"), 0);
    for (int value : {32000, 44100, 48000, 88200, 96000}) {
        if (format == "mp3" && value > 48000) continue;
        if (format == "opus" && value != 48000) continue;
        rate->addItem(QString::number(value) + " Hz", value);
    }
    rate->setCurrentIndex(rate->findData(current.sampleRate));
    rate->setEnabled(format != "opus");
    form->addRow(i18n::text("Frecuencia de muestreo"), rate);
    auto *channels = new QComboBox;
    channels->addItem(i18n::text("Conservar originales"), 0);
    channels->addItem("Mono", 1);
    channels->addItem(i18n::text("Estéreo"), 2);
    channels->setCurrentIndex(channels->findData(current.channels));
    form->addRow(i18n::text("Canales"), channels);
    auto *depth = new QComboBox;
    for (int value : {16, 24, 32}) {
        if (format == "flac" && value == 32) continue;
        depth->addItem(QString::number(value) + " bits", value);
    }
    depth->setCurrentIndex(depth->findData(current.bitDepth));
    depth->setEnabled(format == "wav" || format == "flac");
    form->addRow(i18n::text("Profundidad de bits"), depth);
    layout->addLayout(form);
    auto *info = new QLabel(i18n::text("MP3: la calidad se ajusta con la tasa de bits. WAV y FLAC permiten elegir la profundidad.\nAumentar estos valores no recupera calidad perdida en el archivo original."));
    info->setWordWrap(true);
    layout->addWidget(info);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(i18n::text("Guardar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(i18n::text("Cancelar"));
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;
    options->bitrate = bitrate->currentData().toInt();
    options->sampleRate = rate->currentData().toInt();
    options->channels = channels->currentData().toInt();
    options->bitDepth = depth->currentData().toInt();
    return true;
}

class FormatDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        auto *editor = new QComboBox(parent);
        for (const auto &format : formats(Source(index.data(Qt::UserRole).toInt()) == Source::Spotify))
            editor->addItem(format.toUpper(), format);
        auto *delegate = const_cast<FormatDelegate *>(this);
        connect(editor, qOverload<int>(&QComboBox::activated), delegate, [delegate, editor] {
            emit delegate->commitData(editor);
            emit delegate->closeEditor(editor);
        });
        QTimer::singleShot(0, editor, [editor] {
            if (editor->isVisible()) editor->showPopup();
        });
        return editor;
    }

    void setEditorData(QWidget *widget, const QModelIndex &index) const override
    {
        auto *editor = static_cast<QComboBox *>(widget);
        editor->setCurrentIndex(editor->findData(index.data(Qt::EditRole)));
    }

    void setModelData(QWidget *widget, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QComboBox *>(widget)->currentData(), Qt::EditRole);
    }
};
}

MainWindow::MainWindow()
{
    setWindowTitle(i18n::text("Convert · Audio y vídeo"));
    setObjectName("ConvertWindow");
    setWindowIcon(QIcon(":/convert.png"));
    resize(1180, 760);
    setMinimumSize(850, 580);
    setAcceptDrops(true);
    m_model = new JobModel(this);
    m_runner = new Runner(m_model, this);
    m_updates = new UpdateManager([this] { return m_runner->busy(); }, this);

    auto *fileMenu = menuBar()->addMenu(i18n::text("&Archivo"));
    m_paste = new QAction(i18n::text("Pegar en la lista"), this);
    m_paste->setShortcuts(QKeySequence::Paste);
    m_paste->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    fileMenu->addAction(m_paste);
    fileMenu->addSeparator();
    fileMenu->addAction(i18n::text("Salir"), QKeySequence::Quit, this, &QWidget::close);
    m_preferences = menuBar()->addAction(i18n::text("Preferencias…"), this, &MainWindow::preferences);
    auto *helpMenu = menuBar()->addMenu(i18n::text("Ayuda"));
    m_updateAction = helpMenu->addAction(i18n::text("Actualizaciones…"), this, &MainWindow::showUpdates);
    helpMenu->addSeparator();
    helpMenu->addAction(i18n::text("Acerca de Convert"), this, [this] {
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
    brandText->addWidget(new QLabel(i18n::text("Conversión de audio y vídeo · Descargas")));
    brand->addLayout(brandText);
    brand->addStretch();
    layout->addLayout(brand);

    auto *outputBox = new QFrame;
    outputBox->setProperty("panel", true);
    auto *outputLayout = new QVBoxLayout(outputBox);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(0);
    auto *outputTitle = new QLabel(i18n::text("Formato y carpeta de descargas"));
    outputTitle->setObjectName("PanelTitle");
    outputLayout->addWidget(outputTitle);
    auto *outputContent = new QWidget;
    auto *outputRow = new QHBoxLayout(outputContent);
    outputRow->setContentsMargins(10, 10, 10, 10);
    outputLayout->addWidget(outputContent);
    m_format = new QComboBox;
    m_format->addItems(formats());
    m_format->setCurrentText(QSettings().value("output/format", "mp3").toString());
    QSettings audioSettings;
    m_audio = readAudio(audioSettings, "audio/");
    auto *audio = new QPushButton("Audio…");
    const auto updateAudioButton = [this, audio] {
        audio->setEnabled(isAudio(m_format->currentText()));
        audio->setToolTip(audioDescription(m_format->currentText(), m_audio));
    };
    connect(m_format, &QComboBox::currentTextChanged, this, updateAudioButton);
    connect(audio, &QPushButton::clicked, this, [this, updateAudioButton] {
        if (editAudio(this, m_format->currentText(), &m_audio)) {
            updateAudioButton();
            saveQueue();
        }
    });
    updateAudioButton();
    const auto savedDestination = QSettings().value("output/directory").toString();
    m_destination = new QLineEdit(savedDestination.trimmed().isEmpty()
        ? QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("salida"))
        : savedDestination);
    m_destination->setToolTip(i18n::text("Destino de los enlaces y de cualquier entrada a la que pulses Aplicar a selección. Los archivos nuevos usan su carpeta de origen."));
    auto *browse = new QPushButton(i18n::text("Examinar…"));
    auto *apply = new QPushButton(i18n::text("Aplicar a selección"));
    apply->setToolTip(i18n::text("Aplica el formato, las opciones de audio y la carpeta Salida URL a todas las entradas seleccionadas, incluidos los archivos locales."));
    outputRow->addWidget(new QLabel(i18n::text("Formato")));
    outputRow->addWidget(m_format);
    outputRow->addWidget(audio);
    outputRow->addSpacing(12);
    outputRow->addWidget(new QLabel(i18n::text("Salida URL")));
    outputRow->addWidget(m_destination, 1);
    outputRow->addWidget(browse);
    outputRow->addWidget(apply);
    layout->addWidget(outputBox);
    auto *instructions = new QLabel(i18n::text("Arrastra archivos o pega enlaces con Ctrl+V. Pulsa el formato de una fila para cambiarlo.\nArchivos: carpeta de origen. Enlaces: carpeta de descargas. Los completados salen de la lista."));
    instructions->setWordWrap(true);
    layout->addWidget(instructions);

    auto *splitter = new QSplitter(Qt::Vertical);
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_table->setItemDelegateForColumn(2, new FormatDelegate(m_table));
    connect(m_table, &QTableView::clicked, this, [this](const QModelIndex &index) {
        if (index.column() == 2 && (index.flags() & Qt::ItemIsEditable)) m_table->edit(index);
    });
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->setAccessibleName(i18n::text("Lista de archivos y enlaces"));
    m_table->setToolTip(i18n::text("Arrastra archivos o pulsa Ctrl+V para pegar enlaces. También puedes usar el menú del botón derecho."));
    m_table->addAction(m_paste);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->show();
    m_table->verticalHeader()->setMinimumWidth(36);
    m_table->verticalHeader()->setDefaultSectionSize(34);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 110);
    m_table->setAcceptDrops(true);
    m_table->viewport()->setAcceptDrops(true);
    m_table->viewport()->installEventFilter(this);
    m_emptyHint = new QLabel(i18n::text("Arrastra aquí tus archivos\no pega una URL de YouTube o Spotify\nCtrl+V · Botón derecho → Pegar"), m_table->viewport());
    m_emptyHint->setObjectName("ListEmptyHint");
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyHint->setGeometry(m_table->viewport()->rect());
    splitter->addWidget(m_table);
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1800);
    m_log->setPlaceholderText(i18n::text("Registro de actividad. Los errores de las herramientas aparecerán aquí."));
    auto *logPanel = new QWidget;
    auto *logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(4);
    auto *logHeader = new QHBoxLayout;
    auto *logTitle = new QLabel(i18n::text("Registro de actividad"));
    logTitle->setObjectName("PanelTitle");
    auto *clearLogButton = new QPushButton(i18n::text("Borrar registro"));
    clearLogButton->setToolTip(i18n::text("Vacía el registro mostrado. Los nuevos mensajes seguirán apareciendo."));
    auto *clearLog = new QAction(i18n::text("Borrar registro"), m_log);
    clearLog->setEnabled(false);
    clearLogButton->setEnabled(false);
    connect(clearLog, &QAction::triggered, m_log, &QPlainTextEdit::clear);
    connect(clearLogButton, &QPushButton::clicked, clearLog, &QAction::trigger);
    connect(m_log, &QPlainTextEdit::textChanged, this, [this, clearLog, clearLogButton] {
        const bool hasText = !m_log->document()->isEmpty();
        clearLog->setEnabled(hasText);
        clearLogButton->setEnabled(hasText);
    });
    m_log->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_log, &QWidget::customContextMenuRequested, this, [this, clearLog](const QPoint &position) {
        auto *menu = m_log->createStandardContextMenu();
        menu->addSeparator();
        menu->addAction(clearLog);
        menu->exec(m_log->mapToGlobal(position));
        delete menu;
    });
    logHeader->addWidget(logTitle, 1);
    logHeader->addWidget(clearLogButton);
    logLayout->addLayout(logHeader);
    logLayout->addWidget(m_log, 1);
    splitter->addWidget(logPanel);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({400, 100});
    layout->addWidget(splitter, 1);

    auto *actions = new QHBoxLayout;
    auto *remove = new QPushButton(i18n::text("Quitar selección"));
    auto *retry = new QPushButton(i18n::text("Reintentar selección"));
    auto *open = new QPushButton(i18n::text("Abrir destino"));
    m_start = new QPushButton(i18n::text("Iniciar cola"));
    m_start->setObjectName("primary");
    m_stop = new QPushButton(i18n::text("Detener cola"));
    m_stop->setEnabled(false);
    for (auto *button : {remove, retry, open}) actions->addWidget(button);
    actions->addStretch();
    actions->addWidget(m_stop);
    actions->addWidget(m_start);
    layout->addLayout(actions);
    m_summary = new QLabel;
    m_progress = new QProgressBar;
    m_progress->setMaximumWidth(220);
    statusBar()->addWidget(m_summary, 1);
    statusBar()->addPermanentWidget(m_progress);
    m_updateNotice = new QPushButton(i18n::text("Actualizaciones"));
    m_updateNotice->setVisible(false);
    statusBar()->addPermanentWidget(m_updateNotice);
    connect(m_updateNotice, &QPushButton::clicked, this, &MainWindow::showUpdates);
    setCentralWidget(central);
    m_controls = {outputBox, remove, retry};
    connect(m_paste, &QAction::triggered, this, &MainWindow::pasteFromClipboard);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updatePasteAction);
    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        updatePasteAction();
        QMenu menu(this);
        menu.addAction(m_paste);
        menu.addSeparator();
        auto *audioOptions = menu.addAction(i18n::text("Opciones de audio…"));
        const auto rows = selectedRows();
        bool audioSelection = !rows.isEmpty();
        for (int row : rows) audioSelection &= isAudio(m_model->jobs[row].format);
        audioOptions->setEnabled(!queueBusy() && audioSelection);
        connect(audioOptions, &QAction::triggered, this, &MainWindow::changeSelectedAudio);
        auto *changeDestination = menu.addAction(i18n::text("Cambiar carpeta de salida…"));
        changeDestination->setEnabled(!queueBusy() && !selectedRows().isEmpty());
        connect(changeDestination, &QAction::triggered, this, &MainWindow::changeSelectedDestination);
        menu.addSeparator();
        auto *clearList = menu.addAction(i18n::text("Borrar lista"));
        clearList->setToolTip(i18n::text("Quita todas las entradas de la cola. Conserva los archivos del disco."));
        clearList->setEnabled(!queueBusy() && m_model->rowCount() > 0);
        connect(clearList, &QAction::triggered, this, [this] {
            if (queueBusy()) return;
            m_model->clear();
            saveQueue();
            m_table->setFocus();
        });
        menu.exec(m_table->viewport()->mapToGlobal(position));
    });
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto dir = QFileDialog::getExistingDirectory(this, i18n::text("Carpeta de salida"), m_destination->text());
        if (!dir.isEmpty()) m_destination->setText(dir);
    });
    connect(apply, &QPushButton::clicked, this, [this] {
        const auto rows = selectedRows();
        if (queueBusy() || rows.isEmpty()) return;
        for (int row : rows) {
            const auto &j = m_model->jobs[row];
            if (j.state == State::Done) continue;
            if (j.kind == Source::Spotify && !isAudio(m_format->currentText())) {
                QMessageBox::information(this, i18n::text("Formato de Spotify"), i18n::text("Selecciona un formato de audio para las entradas de Spotify."));
                return;
            }
        }
        const auto destination = outputDirectory(m_destination->text());
        if (destination.isEmpty()) {
            QMessageBox::information(this, i18n::text("Destino"), i18n::text("Selecciona una carpeta en Salida URL.")); return;
        }
        for (int row : rows) {
            auto &j = m_model->jobs[row];
            if (j.state == State::Done) continue;
            j.format = m_format->currentText();
            j.audio = m_audio;
            j.destination = destination;
            j.customDestination = true;
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
    connect(m_updates, &UpdateManager::log, m_log, &QPlainTextEdit::appendPlainText);
    connect(m_updates, &UpdateManager::installationReport, this, [this](const QString &message, bool failed) {
        if (failed) QMessageBox::warning(this, i18n::text("Actualización de Convert"), message);
        else statusBar()->showMessage(message, 15000);
    });
    connect(m_updates, &UpdateManager::available, this, [this](int count) {
        m_updateNotice->setVisible(count > 0);
        m_updateNotice->setText(i18n::text("Actualizaciones (%1)").arg(count));
        m_updateAction->setText(count > 0 ? i18n::text("Actualizaciones (%1 disponibles)…").arg(count)
                                       : i18n::text("Actualizaciones…"));
    });
    connect(m_updates, &UpdateManager::installationChanged, this, [this](bool active) {
        m_updateInstalling = active;
        setBusy(m_runner->busy());
    });
    connect(m_updates, &UpdateManager::restartReady, this, [this] {
        m_restartForUpdate = true;
        QSettings().setValue("session/restoreAfterUpdate", true);
        saveQueue();
        close();
    });
    connect(m_model, &QAbstractItemModel::dataChanged, this, &MainWindow::updateSummary);
    connect(m_model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
        if (roles.contains(Qt::EditRole)) saveQueue();
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateSummary);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateSummary);
    restoreQueue();
    restoreGeometry(QSettings().value("window/geometry").toByteArray());
    updateSummary();
    updatePasteAction();
    m_table->setFocus();
    m_log->appendPlainText(i18n::text("Preparado. Arrastra archivos a la lista o pega enlaces con Ctrl+V."));
    QTimer::singleShot(3000, m_updates, &UpdateManager::checkAutomatically);
}

bool MainWindow::queueBusy() const
{
    return m_runner->busy() || m_updateInstalling;
}

void MainWindow::showUpdates()
{
    if (!m_updateDialog) {
        m_updateDialog = new UpdateDialog(m_updates, this);
        m_updateDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_updateDialog->setModal(true);
    }
    m_updateDialog->show();
    m_updateDialog->raise();
    m_updateDialog->activateWindow();
    if (!m_updates->busy() && m_updates->items().first().status == i18n::text("Sin comprobar")) m_updates->check();
}

QList<int> MainWindow::selectedRows() const
{
    QList<int> rows;
    for (const auto &index : m_table->selectionModel()->selectedRows()) rows << index.row();
    return rows;
}

void MainWindow::addFiles(const QStringList &files)
{
    if (queueBusy()) return;
    for (const auto &file : files) {
        const QFileInfo info(file);
        if (!info.isFile()) { m_log->appendPlainText(i18n::text("Omitido (no es un archivo): ") + file); continue; }
        Job j;
        j.source = info.absoluteFilePath(); j.format = m_format->currentText();
        j.audio = m_audio;
        j.destination = info.absolutePath();
        m_model->append(j);
    }
    saveQueue();
}

bool MainWindow::addUrl(const QString &text, QString *error)
{
    Source source;
    if (!classifyUrl(text, &source)) { *error = i18n::text("URL no admitida: ") + text; return false; }
    if (source == Source::Spotify && !isAudio(m_format->currentText())) {
        *error = i18n::text("Spotify requiere MP3, WAV, FLAC, M4A, OPUS u OGG."); return false;
    }
    const auto destination = outputDirectory(m_destination->text());
    if (destination.isEmpty()) { *error = i18n::text("Selecciona una carpeta de salida."); return false; }
    Job j;
    j.source = text; j.kind = source; j.format = m_format->currentText();
    j.audio = m_audio;
    j.destination = destination;
    m_model->append(j);
    return true;
}

void MainWindow::pasteFromClipboard()
{
    if (queueBusy()) return;
    const auto *data = QApplication::clipboard()->mimeData();
    if (data) handleDrop(data);
}

void MainWindow::updatePasteAction()
{
    const auto *data = QApplication::clipboard()->mimeData();
    m_paste->setEnabled(!queueBusy() && data && (data->hasUrls() || data->hasText()));
}

void MainWindow::changeSelectedDestination()
{
    if (queueBusy()) return;
    const auto rows = selectedRows();
    if (rows.isEmpty()) return;
    const auto initial = m_model->jobs[rows.first()].destination;
    const auto folder = QFileDialog::getExistingDirectory(this, i18n::text("Cambiar carpeta de salida"), initial);
    if (folder.isEmpty()) return;
    for (int row : rows) {
        auto &job = m_model->jobs[row];
        job.destination = outputDirectory(folder);
        job.customDestination = true;
        job.state = State::Pending;
        job.progress = 0;
        job.result.clear();
        job.detail.clear();
        m_model->refresh(row);
    }
    saveQueue();
}

void MainWindow::changeSelectedAudio()
{
    if (queueBusy()) return;
    const auto rows = selectedRows();
    if (rows.isEmpty()) return;
    const auto format = m_model->jobs[rows.first()].format;
    for (int row : rows) {
        if (m_model->jobs[row].format != format) {
            QMessageBox::information(this, i18n::text("Opciones de audio"), i18n::text("Selecciona entradas con el mismo formato de audio."));
            return;
        }
    }
    if (!isAudio(format)) return;
    auto options = m_model->jobs[rows.first()].audio;
    if (!editAudio(this, format, &options)) return;
    for (int row : rows) {
        auto &job = m_model->jobs[row];
        job.audio = options;
        job.state = State::Pending;
        job.progress = 0;
        job.result.clear();
        job.detail.clear();
        m_model->refresh(row);
    }
    saveQueue();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!queueBusy() && (event->mimeData()->hasUrls() || event->mimeData()->hasText())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void MainWindow::handleDrop(const QMimeData *data)
{
    if (queueBusy()) return;
    QStringList files, links, errors;
    if (data->hasUrls()) {
        for (const auto &url : data->urls()) {
            if (url.isLocalFile()) files << url.toLocalFile();
            else links << url.toString();
        }
    } else links = data->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (!files.isEmpty()) addFiles(files);
    for (const auto &link : links) { QString error; if (!addUrl(link, &error)) errors << error; }
    if (!errors.isEmpty()) QMessageBox::information(this, i18n::text("Elementos omitidos"), errors.join('\n'));
    saveQueue();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (queueBusy()) return;
    handleDrop(event->mimeData());
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_table->viewport()) {
        if (event->type() == QEvent::Resize && m_emptyHint)
            m_emptyHint->setGeometry(m_table->viewport()->rect());
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *drag = static_cast<QDragMoveEvent *>(event);
            if (!queueBusy() && (drag->mimeData()->hasUrls() || drag->mimeData()->hasText())) {
                drag->setDropAction(Qt::CopyAction);
                drag->accept();
            } else drag->ignore();
            return true;
        }
        if (event->type() == QEvent::Drop) { dropEvent(static_cast<QDropEvent *>(event)); return true; }
    }
    return QMainWindow::eventFilter(object, event);
}

void MainWindow::setBusy(bool busy)
{
    busy = busy || m_updateInstalling;
    m_model->setProcessing(busy);
    for (auto *control : m_controls) control->setEnabled(!busy);
    m_preferences->setEnabled(!busy);
    m_stop->setEnabled(m_runner->busy());
    updatePasteAction();
    saveQueue();
    updateSummary();
    if (!busy && m_closeAfterCancel) QTimer::singleShot(0, this, &QWidget::close);
}

void MainWindow::updateSummary()
{
    m_emptyHint->setVisible(m_model->jobs.isEmpty());
    int done = m_runner->completedCount(), pending = 0, failed = 0;
    int current = 0;
    for (const auto &j : m_model->jobs) {
        pending += j.state == State::Pending;
        failed += j.state == State::Failed;
        if (j.state == State::Running) current = j.progress;
    }
    m_summary->setText(i18n::text("%1 en lista · %2 pendientes · %3 completadas · %4 errores · %5 total")
        .arg(m_model->jobs.size()).arg(pending).arg(done).arg(failed).arg(m_model->jobs.size() + done));
    m_start->setEnabled(!queueBusy() && pending > 0);
    m_progress->setRange(0, current < 0 ? 0 : 100);
    m_progress->setValue(queueBusy() ? qMax(0, current) : (done > 0 && pending == 0 ? 100 : 0));
}

void MainWindow::preferences()
{
    QDialog dialog(this);
    dialog.setWindowTitle(i18n::text("Preferencias · Convert"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(i18n::text("Selecciona ejecutables o deja el campo vacío para buscar primero en la carpeta tools junto a Convert.\nDespués se busca en bin, ffmpeg, junto al programa y en PATH.\nFFmpeg es obligatorio. ffprobe permite calcular el progreso de conversión.\nDeno proporciona el motor JavaScript que necesita yt-dlp para YouTube."));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *form = new QFormLayout;
    QMap<QString, QLineEdit *> entries;
    auto *language = new QComboBox;
    language->addItem("English", "en");
    language->addItem("Castellano", "es");
    const QString previousLanguage = QSettings().value("interface/language", "en").toString() == "es" ? "es" : "en";
    language->setCurrentIndex(language->findData(previousLanguage));
    language->setToolTip(i18n::text("El cambio de idioma se aplicará al volver a abrir Convert."));
    form->addRow("Language / Idioma", language);
    auto *languageInfo = new QLabel(i18n::text("El cambio de idioma se aplicará al volver a abrir Convert."));
    languageInfo->setWordWrap(true);
    form->addRow(languageInfo);
    for (const auto &name : QStringList{"ffmpeg", "ffprobe", "yt-dlp", "spotdl", "deno"}) {
        const auto detected = toolPath(name); // Also migrates an old local bin path.
        auto *row = new QHBoxLayout;
        auto *edit = new QLineEdit(QSettings().value("tools/" + name).toString());
        edit->setPlaceholderText(detected.isEmpty() ? i18n::text("No encontrado") : detected);
        edit->setMinimumWidth(400);
        auto *browse = new QPushButton(i18n::text("Examinar…"));
        row->addWidget(edit); row->addWidget(browse);
        form->addRow(name, row); entries.insert(name, edit);
        connect(browse, &QPushButton::clicked, &dialog, [&dialog, edit] {
            const auto path = QFileDialog::getOpenFileName(&dialog, i18n::text("Seleccionar ejecutable"), edit->text());
            if (!path.isEmpty()) edit->setText(path);
        });
    }
    layout->addLayout(form);
    auto *rememberQueue = new QCheckBox(i18n::text("Recordar la cola entre sesiones"));
    rememberQueue->setChecked(QSettings().value("session/rememberQueue", false).toBool());
    rememberQueue->setToolTip(i18n::text("Desactivado: Convert siempre abre con la lista vacía. Las rutas y el formato se conservan."));
    layout->addWidget(rememberQueue);
    auto *info = new QLabel(i18n::text("Spotify utiliza spotDL como herramienta externa; el audio procede de YouTube.\nLas descargas pueden depender de disponibilidad, autenticación y cambios del servicio."));
    info->setWordWrap(true); layout->addWidget(info);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(i18n::text("Guardar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(i18n::text("Cancelar"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, entries, rememberQueue, language] {
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            const auto path = it.value()->text().trimmed();
            if (!path.isEmpty() && (!QFileInfo(path).isFile() || !QFileInfo(path).isExecutable())) {
                QMessageBox::warning(&dialog, i18n::text("Ruta incorrecta"), i18n::text("Revisa el ejecutable de ") + it.key()); return;
            }
        }
        QSettings settings;
        for (auto it = entries.cbegin(); it != entries.cend(); ++it)
            settings.setValue("tools/" + it.key(), it.value()->text().trimmed());
        settings.setValue("session/rememberQueue", rememberQueue->isChecked());
        settings.setValue("interface/language", language->currentData().toString());
        dialog.accept();
    });
    if (dialog.exec() == QDialog::Accepted) {
        saveQueue();
        if (language->currentData().toString() != previousLanguage)
            QMessageBox::information(this, i18n::text("Idioma guardado"),
                i18n::text("Idioma guardado. Cierra y vuelve a abrir Convert para aplicarlo."));
    }
}

void MainWindow::saveQueue()
{
    QSettings s;
    s.setValue("output/format", m_format->currentText());
    s.setValue("output/directory", m_destination->text());
    writeAudio(s, "audio/", m_audio);
    if (!s.value("session/rememberQueue", false).toBool() && !s.value("session/restoreAfterUpdate", false).toBool()) {
        s.remove("queue");
        return;
    }
    s.beginWriteArray("queue", m_model->jobs.size());
    for (int row = 0; row < m_model->jobs.size(); ++row) {
        const auto &j = m_model->jobs[row];
        s.setArrayIndex(row);
        s.setValue("source", j.source); s.setValue("kind", int(j.kind));
        s.setValue("format", j.format); s.setValue("destination", j.destination);
        writeAudio(s, "audio/", j.audio);
        s.setValue("customDestination", j.customDestination);
        s.setValue("state", int(j.state)); s.setValue("result", j.result);
        s.setValue("detail", j.detail);
    }
    s.endArray();
}

void MainWindow::restoreQueue()
{
    QSettings s;
    if (!s.value("session/rememberQueue", false).toBool() && !s.value("session/restoreAfterUpdate", false).toBool()) {
        s.remove("queue");
        return;
    }
    const int size = s.beginReadArray("queue");
    for (int row = 0; row < size; ++row) {
        s.setArrayIndex(row);
        Job j;
        j.source = s.value("source").toString();
        const int kind = s.value("kind").toInt();
        if (kind < 0 || kind > 2) continue;
        j.kind = Source(kind); j.format = s.value("format").toString();
        j.audio = readAudio(s, "audio/");
        j.destination = s.value("destination").toString();
        j.customDestination = s.value("customDestination", false).toBool();
        if (!formats(j.kind == Source::Spotify).contains(j.format) || j.destination.isEmpty()) continue;
        Source parsed;
        if (j.kind != Source::File && (!classifyUrl(j.source, &parsed) || parsed != j.kind)) continue;
        const int state = s.value("state").toInt();
        j.state = state >= 0 && state <= 4 ? State(state) : State::Pending;
        if (j.state == State::Done) continue;
        if (j.state == State::Running) j.state = State::Cancelled;
        if (j.kind == Source::File && !j.customDestination)
            j.destination = QFileInfo(j.source).absolutePath();
        j.result = s.value("result").toString(); j.detail = s.value("detail").toString();
        j.progress = j.state == State::Done ? 100 : 0;
        m_model->append(j);
    }
    s.endArray();
    if (s.value("session/restoreAfterUpdate", false).toBool()) {
        s.setValue("session/restoreAfterUpdate", false);
        if (!s.value("session/rememberQueue", false).toBool()) s.remove("queue");
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_updateInstalling && !m_restartForUpdate) {
        QMessageBox::information(this, i18n::text("Actualización en curso"), i18n::text("Termina o cancela la actualización desde Ayuda > Actualizaciones antes de cerrar."));
        event->ignore();
        return;
    }
    if (m_runner->busy()) {
        const auto answer = QMessageBox::question(this, i18n::text("Trabajo en curso"),
            i18n::text("¿Detener la cola y cerrar? Las descargas parciales permanecerán en su carpeta."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        event->ignore();
        if (answer == QMessageBox::Yes) { m_closeAfterCancel = true; m_runner->cancel(); }
        return;
    }
    saveQueue();
    QSettings().setValue("window/geometry", saveGeometry());
    event->accept();
}
