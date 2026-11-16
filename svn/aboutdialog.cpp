#include "aboutdialog.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName("ConvertAbout");
    setWindowTitle("Acerca de Convert");
    setWindowIcon(QIcon(":/convert.png"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setMinimumSize(760, 350);
    resize(900, 380);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 12, 28, 24);
    auto *top = new QHBoxLayout;
    top->addStretch();
    auto *close = new QPushButton(QString::fromUtf8("×"));
    close->setObjectName("AboutClose");
    close->setFixedSize(28, 28);
    close->setToolTip("Cerrar (Esc)");
    close->setAccessibleName("Cerrar Acerca de Convert");
    close->setAutoDefault(false);
    top->addWidget(close);
    outer->addLayout(top);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(26);
    const auto text = [this](const QString &value, const QString &name) {
        auto *label = new QLabel(value, this);
        label->setObjectName(name);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        return label;
    };
    auto *description = new QVBoxLayout;
    description->setSpacing(12);
    description->addStretch();
    description->addWidget(text("Conversión de audio y vídeo.\nDescargas de YouTube y enlaces de Spotify.", "AboutDescription"));
    description->addSpacing(8);
    description->addWidget(text("Desarrollado por", "AboutMuted"));
    description->addWidget(text("Victor Algaba", "AboutAuthor"));
    description->addStretch();
    auto *website = text("<a href=\"https://www.radit.org\" style=\"color:#80A4AE;text-decoration:none\">www.radit.org</a>", "AboutWebsite");
    website->setOpenExternalLinks(true);
    description->addWidget(website);
    description->addWidget(text("Identidad visual de la familia Radit", "AboutMuted"));
    columns->addLayout(description, 1);

    auto *identity = new QVBoxLayout;
    identity->setSpacing(4);
    identity->addStretch();
    auto *logo = text({}, "AboutLogo");
    logo->setFixedSize(190, 190);
    logo->setPixmap(QIcon(":/convert.png").pixmap(QSize(190, 190), devicePixelRatioF()));
    logo->setAccessibleName("Logotipo de Convert: letra C");
    identity->addWidget(logo, 0, Qt::AlignHCenter);
    identity->addWidget(text("convert", "AboutName"));
    identity->addStretch();
    identity->addWidget(text("Versión " + QCoreApplication::applicationVersion(), "AboutVersion"));
    columns->addLayout(identity, 1);

    auto *credits = new QVBoxLayout;
    credits->setSpacing(7);
    credits->addStretch();
    for (const auto &item : QList<QPair<QString, QString>>{
             {"Conversión de audio y vídeo", "FFmpeg"},
             {"Descargas de YouTube", "yt-dlp"},
             {"Enlaces y metadatos de Spotify", "spotDL"},
             {"Interfaz de escritorio", "Qt / C++"}}) {
        credits->addWidget(text(item.first, "AboutMuted"));
        credits->addWidget(text(item.second, "AboutCredits"));
        credits->addSpacing(4);
    }
    const auto build = QFileInfo(QCoreApplication::applicationFilePath()).lastModified();
    if (build.isValid()) credits->addWidget(text("Compilación: " + build.toString("dd/MM/yyyy HH:mm"), "AboutVersion"));
    credits->addStretch();
    credits->addWidget(text(QString::fromUtf8("© %1 Victor Algaba").arg(QDate::currentDate().year()), "AboutCopyright"));
    columns->addLayout(credits, 1);
    outer->addLayout(columns, 1);
}

void AboutDialog::paintEvent(QPaintEvent *event)
{
    QDialog::paintEvent(event);
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    const qreal y = height() * 0.67;
    painter.setBrush(QColor(56, 176, 209, 32));
    painter.drawRect(QRectF(0, y, width() * 0.32, 4));
    painter.setBrush(QColor(56, 176, 209, 14));
    painter.drawRect(QRectF(width() * 0.68, y, width() * 0.32, 2));
    painter.setBrush(QColor("#38b0d1"));
    painter.drawRect(QRectF(0, y, 26, 4));
}

void AboutDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    const QRect available = screen()->availableGeometry();
    const QRect parentBounds = parentWidget() ? parentWidget()->window()->frameGeometry() : available;
    const QPoint desired = parentBounds.center() - rect().center();
    move(qBound(available.left(), desired.x(), qMax(available.left(), available.right() - width() + 1)),
         qBound(available.top(), desired.y(), qMax(available.top(), available.bottom() - height() + 1)));
}
