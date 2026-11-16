#pragma once

#include <QAbstractTableModel>
#include <QStringList>

enum class Source { File, YouTube, Spotify };
enum class State { Pending, Running, Done, Failed, Cancelled };

struct AudioOptions {
    int bitrate = 192; // kb/s
    int sampleRate = 0; // 0: automático
    int channels = 0; // 0: conservar
    int bitDepth = 16;
};

struct Job {
    QString source;
    Source kind = Source::File;
    QString format;
    AudioOptions audio;
    QString destination;
    bool customDestination = false;
    State state = State::Pending;
    int progress = 0;
    QString detail;
    QString result;
};

QStringList formats(bool audioOnly = false);
bool isAudio(const QString &format);
AudioOptions normalizedAudio(const QString &format, AudioOptions options);
QStringList audioArguments(const QString &format, AudioOptions options);
QString audioDescription(const QString &format, AudioOptions options);
QString toolPath(const QString &name);
QString outputDirectory(const QString &text);
bool classifyUrl(const QString &text, Source *kind);
QStringList conversionArguments(const QString &input, const QString &output,
                                const QString &format, AudioOptions options,
                                bool overwriteReservedFile = false);

class JobModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit JobModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void append(const Job &job);
    void remove(int row);
    void clear();
    void refresh(int row);
    void setProcessing(bool processing);
    QList<Job> jobs;
private:
    bool m_processing = false;
};
