#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
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
