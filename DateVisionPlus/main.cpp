#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DateVisionPlus"));
    app.setOrganizationName(QStringLiteral("DateVisionPlus"));
    app.setApplicationVersion(QStringLiteral("2.0.5"));

    // 默认字体与 Fusion 暗色主题
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    app.setStyle(QStringLiteral("Fusion"));

    // Supply the same dark colors to native delegates and unstyled plugin widgets.
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#151c22"));
    palette.setColor(QPalette::WindowText, QColor("#e2ebf0"));
    palette.setColor(QPalette::Base, QColor("#10171d"));
    palette.setColor(QPalette::AlternateBase, QColor("#182129"));
    palette.setColor(QPalette::Text, QColor("#e2ebf0"));
    palette.setColor(QPalette::Button, QColor("#1c272f"));
    palette.setColor(QPalette::ButtonText, QColor("#e2ebf0"));
    palette.setColor(QPalette::Highlight, QColor("#b7d2df"));
    palette.setColor(QPalette::HighlightedText, QColor("#101519"));
    palette.setColor(QPalette::ToolTipBase, QColor("#1c272f"));
    palette.setColor(QPalette::ToolTipText, QColor("#e2ebf0"));
    palette.setColor(QPalette::PlaceholderText, QColor("#8195a3"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#60717c"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#60717c"));
    app.setPalette(palette);

    QFile styleFile(QStringLiteral(":/dark.qss"));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = QStringLiteral("datevisionplus_") + QLocale(locale).name();
        if (translator.load(QStringLiteral(":/i18n/") + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    MainWindow w;
    w.show();

    return app.exec();
}
