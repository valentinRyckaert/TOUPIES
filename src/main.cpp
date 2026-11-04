#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ToupieArena"));
    QApplication::setOrganizationName(QStringLiteral("ToupieArena"));
    QApplication::setApplicationDisplayName(QStringLiteral("Toupie Arena"));
    QApplication::setApplicationVersion(QStringLiteral(TOUPIE_VERSION));

    toupie::Theme::apply(app);

    toupie::MainWindow window;
    window.show();

    // Vérification d'intégration (CI) : l'application démarre puis se ferme.
    if (QCoreApplication::arguments().contains(QStringLiteral("--smoke-test")))
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);

    return app.exec();
}
