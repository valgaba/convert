#pragma once

#include <QAbstractTableModel>
#include <QStringList>

enum class Source { File, YouTube, Spotify };
enum class State { Pending, Running, Done, Failed, Cancelled };

struct Job {
    QString source;
    Source kind = Source::File;
    QString format;
    QString destination;
    State state = State::Pending;
    int progress = 0;
    QString detail;
    QString result;
};

QStringList formats(bool audioOnly = false);
bool isAudio(const QString &format);
QString toolPath(const QString &name);
QString outputDirectory(const QString &text);
bool classifyUrl(const QString &text, Source *kind);
QStringList conversionArguments(const QString &input, const QString &output,
                                const QString &format);

class JobModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit JobModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void append(const Job &job);
    void remove(int row);
    void refresh(int row);
    QList<Job> jobs;
};
