QT += core gui widgets network

TEMPLATE = app
TARGET = convert
VERSION = $$cat($$PWD/app-version.txt, lines)
DEFINES += CONVERT_VERSION=\\\"$$VERSION\\\"

CONFIG += c++17 warn_on
CONFIG -= debug_and_release console app_bundle

!versionAtLeast(QT_VERSION, 6.7.0) {
    error("Convert requiere Qt 6.7 o posterior.")
}

SOURCES += \
    svn/main.cpp \
    svn/mainwindow.cpp \
    svn/jobs.cpp \
    svn/runner.cpp
SOURCES += svn/aboutdialog.cpp
SOURCES += svn/updates.cpp
SOURCES += svn/i18n.cpp

HEADERS += \
    svn/mainwindow.h \
    svn/jobs.h \
    svn/runner.h
HEADERS += svn/aboutdialog.h
HEADERS += svn/updates.h
HEADERS += svn/i18n.h

RESOURCES += svn/resources.qrc

DESTDIR = "$$PWD/out/bin"
OBJECTS_DIR = "$$OUT_PWD/obj"
MOC_DIR = "$$OUT_PWD/moc"
RCC_DIR = "$$OUT_PWD/rcc"
UI_DIR = "$$OUT_PWD/ui"

win32-g++ {
    QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8
}
win32:RC_ICONS = svn/convert.ico
win32:QMAKE_POST_LINK += powershell.exe -NoProfile -ExecutionPolicy Bypass -File $$shell_quote($$shell_path($$PWD/prepare-runtime.ps1)) -RuntimeDirectory $$shell_quote($$shell_path($$DESTDIR))

DISTFILES += \
    svn/convert.qss \
    README.md \
    build.ps1
DISTFILES += svn/convert.svg svn/convert.png generate-icon.ps1 svn/convert.rc
DISTFILES += svn/splashscreen.png
DISTFILES += prepare-runtime.ps1
DISTFILES += app-version.txt svn/update-install.ps1 package-release.ps1
DISTFILES += svn/english.json svn/qtbase_es.qm
