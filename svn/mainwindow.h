#pragma once
#include <QMainWindow>
#include <QPointer>
#include "jobs.h"
#include "runner.h"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableView;
class QLabel;
class QProgressBar;
class QMimeData;
class QAction;
class QDragEnterEvent;
class QDropEvent;
class QCloseEvent;
class UpdateManager;
class UpdateDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void addFiles(const QStringList &files);
    bool addUrl(const QString &url, QString *error);
    void pasteFromClipboard();
    void updatePasteAction();
    void changeSelectedDestination();
    void changeSelectedAudio();
    void preferences();
    void showUpdates();
    bool queueBusy() const;
    void setBusy(bool busy);
    void updateSummary();
    void saveQueue();
    void restoreQueue();
    void handleDrop(const QMimeData *data);
    QList<int> selectedRows() const;
    JobModel *m_model;
    Runner *m_runner;
    QTableView *m_table;
    QComboBox *m_format;
    AudioOptions m_audio;
    QLineEdit *m_destination;
    QPlainTextEdit *m_log;
    QLabel *m_summary;
    QLabel *m_emptyHint = nullptr;
    QProgressBar *m_progress;
    QPushButton *m_start, *m_stop;
    QList<QWidget *> m_controls;
    QAction *m_preferences, *m_paste;
    bool m_closeAfterCancel = false;
    bool m_updateInstalling = false, m_restartForUpdate = false;
    UpdateManager *m_updates;
    QAction *m_updateAction;
    QPushButton *m_updateNotice;
    QPointer<UpdateDialog> m_updateDialog;
};
