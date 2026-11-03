#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ToupieArena"));
    QApplication::setOrganizationName(QStringLiteral("ToupieArena"));
    QApplication::setApplicationDisplayName(QStringLiteral("Toupie Arena"));

    toupie::Theme::apply(app);

    toupie::MainWindow window;
    window.show();
    return app.exec();
}
