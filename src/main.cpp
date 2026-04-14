#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QWindow>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("ping++"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("ping++"));

    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // Detect dark theme by checking window color brightness
    QPalette palette = app.palette();
    QColor windowColor = palette.color(QPalette::Window);
    bool isDark = windowColor.redF() * 0.299
                + windowColor.greenF() * 0.587
                + windowColor.blueF() * 0.114 < 0.5;

    MainWindow window;
    window.setTheme(isDark ? MainWindow::Theme::Dark : MainWindow::Theme::Light);
    window.show();

    return app.exec();
}
