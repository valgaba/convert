#include "i18n.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <cstring>

namespace {
QString currentLanguage;
class EnglishTranslator : public QTranslator {
public:
    QJsonObject messages;
    bool isEmpty() const override { return messages.isEmpty(); }
    QString translate(const char *context, const char *source, const char *, int) const override
    {
        if (!context || std::strcmp(context, "Convert") != 0 || !source) return {};
        return messages.value(QString::fromUtf8(source)).toString();
    }
};
}

void i18n::initialize()
{
    currentLanguage = QSettings().value("interface/language", "en").toString() == "es" ? "es" : "en";
    QLocale::setDefault(QLocale(currentLanguage == "es" ? "es_ES" : "en_GB"));
    if (currentLanguage == "en") {
        auto *translator = new EnglishTranslator;
        translator->setParent(QCoreApplication::instance());
        QFile catalog(":/english.json");
        if (catalog.open(QIODevice::ReadOnly)) translator->messages = QJsonDocument::fromJson(catalog.readAll()).object();
        QCoreApplication::installTranslator(translator);
    } else {
        auto *translator = new QTranslator(QCoreApplication::instance());
        if (translator->load(":/qtbase_es.qm")) QCoreApplication::installTranslator(translator);
    }
}

QString i18n::language() { return currentLanguage; }
QString i18n::text(const char *source) { return QCoreApplication::translate("Convert", source); }
