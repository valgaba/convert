#include "i18n.h"
#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QStyleFactory>
#include <QSplashScreen>
#include <QElapsedTimer>
#include <QPainter>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("RaditTools");
    app.setApplicationName("Convert");
    app.setApplicationVersion(QStringLiteral(CONVERT_VERSION));
    i18n::initialize();
    app.setWindowIcon(QIcon(":/convert.png"));
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#181e2c"));
    palette.setColor(QPalette::WindowText, QColor("#dfdbd2"));
    palette.setColor(QPalette::Base, QColor("#181e2c"));
    palette.setColor(QPalette::AlternateBase, QColor("#20283a"));
    palette.setColor(QPalette::Text, QColor("#dfdbd2"));
    palette.setColor(QPalette::Button, QColor("#262c3b"));
    palette.setColor(QPalette::ButtonText, QColor("#80A4AE"));
    palette.setColor(QPalette::Highlight, QColor("#4e4d7a"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#686b74"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#686b74"));
    app.setPalette(palette);
    QFile style(":/convert.qss");
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    QPixmap artwork(":/splashscreen.png");
    // The artwork is generated at twice its logical size for high-DPI screens.
    artwork.setDevicePixelRatio(2.0);
    if (!artwork.isNull()) {
        QPainter painter(&artwork);
        painter.setFont(QFont("Segoe UI", 9));
        painter.setPen(QColor("#dfdbd2"));
        painter.drawText(QRect(24, 251, 452, 22), Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1 · Victor Algaba · Convert").arg(app.applicationVersion()));
    }
    QSplashScreen splash(artwork);
    QElapsedTimer startup;
    startup.start();
    if (!artwork.isNull()) {
        splash.setWindowTitle(i18n::text("Iniciando Convert"));
        splash.setWindowIcon(app.windowIcon());
        splash.setCursor(Qt::WaitCursor);
        splash.show();
        app.processEvents();
    }
    MainWindow window;
    // Keep the splash legible briefly without sleeping or blocking the event loop.
    const int delay = artwork.isNull() ? 0 : int(qMax<qint64>(0, 900 - startup.elapsed()));
    QTimer::singleShot(delay, &window, [&window, &splash] {
        window.show();
        splash.finish(&window);
    });
    return app.exec();
}
