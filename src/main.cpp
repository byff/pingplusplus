#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QWindow>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("pingtestpp");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("pingtestpp");

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    // Detect dark theme by checking window color brightness
    QPalette palette = app.palette();
    QColor windowColor = palette.color(QPalette::Window);
    bool isDark = windowColor.redF() * 0.299 + windowColor.greenF() * 0.587 + windowColor.blueF() * 0.114 < 0.5;

    MainWindow window;
    window.setTheme(isDark ? MainWindow::Theme::Dark : MainWindow::Theme::Light);
    window.show();

    return app.exec();
}
