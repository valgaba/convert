#pragma once
#include <QMainWindow>
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
    void addFromField();
    void preferences();
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
    QLineEdit *m_destination, *m_url;
    QPlainTextEdit *m_log;
    QLabel *m_summary;
    QProgressBar *m_progress;
    QPushButton *m_start, *m_stop;
    QList<QWidget *> m_controls;
    QAction *m_preferences;
    bool m_closeAfterCancel = false;
};
