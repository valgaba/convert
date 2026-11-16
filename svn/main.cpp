#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("RaditTools");
    app.setApplicationName("Convert");
    app.setApplicationVersion("0.1.0");
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
    MainWindow window;
    window.show();
    return app.exec();
}
