#pragma once
#include "jobs.h"
#include <QProcess>
#include <QTemporaryDir>
#include <memory>

class Runner : public QObject {
    Q_OBJECT
public:
    explicit Runner(JobModel *model, QObject *parent = nullptr);
    ~Runner() override;
    bool busy() const { return m_busy; }
    void start();
    void cancel();
signals:
    void busyChanged(bool busy);
    void log(const QString &line);
    void checkpoint();
private:
    void next();
    void execute(const QString &program, const QStringList &arguments, bool probe = false);
    void consume(bool final = false);
    void complete(bool success, const QString &message);
    void convert();
    void closeProcessTree();
    bool prepareProcessTemporaryDirectory();
    JobModel *m_model;
    QProcess *m_process = nullptr;
    int m_row = -1;
    bool m_busy = false, m_cancelled = false, m_probe = false;
    bool m_temporaryFailure = false;
    QByteArray m_buffer, m_probeData;
    double m_duration = 0;
    QString m_output, m_staged;
    QStringList m_downloadFiles;
    QString m_spotifyMetadata, m_spotifyErrors;
    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<QTemporaryDir> m_runtimeTemp;
    void *m_processTree = nullptr;
};
