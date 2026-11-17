#pragma once

#include <QDialog>
#include <QObject>
#include <QUrl>
#include <QList>
#include <QPointer>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QCryptographicHash;
class QTableWidget;
class QPushButton;
class QLabel;
class QProgressBar;

struct UpdateItem {
    QString key, name, repository, installed, latest, assetName, digest;
    QString status, localError, remoteError;
    QUrl download, release;
    qint64 size = 0;
    bool missing = false, available = false;
};

class UpdateManager : public QObject {
    Q_OBJECT
public:
    explicit UpdateManager(std::function<bool()> queueBusy, QObject *parent = nullptr);
    ~UpdateManager() override;
    const QList<UpdateItem> &items() const { return m_items; }
    bool busy() const { return m_busy; }
    bool installing() const { return m_installing; }
    bool canCancel() const { return !m_download.isNull() || m_waitingRestart; }
    QString message() const { return m_message; }
    int progress() const { return m_progress; }
    void check();
    void checkAutomatically();
    void install(int row);
    void cancel();
signals:
    void changed();
    void available(int count);
    void installationChanged(bool active);
    void restartReady();
    void installationReport(const QString &message, bool failed);
    void log(const QString &message);
private:
    void probe(int row);
    void fetchRelease(int row);
    void checkedOne();
    void beginInstaller(int row);
    void finishInstallation(const QString &error);
    QNetworkAccessManager *m_network;
    std::function<bool()> m_queueBusy;
    QList<UpdateItem> m_items;
    int m_pending = 0, m_progress = -1;
    bool m_busy = false, m_installing = false, m_cancelled = false, m_waitingRestart = false;
    QString m_message, m_work, m_writeError;
    QPointer<QNetworkReply> m_download;
    QFile *m_package = nullptr;
    QCryptographicHash *m_hash = nullptr;
};

class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(UpdateManager *manager, QWidget *parent = nullptr);
    void reject() override;
private:
    void refresh();
    UpdateManager *m_manager;
    QTableWidget *m_table;
    QPushButton *m_check, *m_install, *m_cancel, *m_release;
    QLabel *m_message;
    QProgressBar *m_progress;
};
