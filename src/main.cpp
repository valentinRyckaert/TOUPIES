#include "ui/Language.h"
#include "ui/MainWindow.h"
#include "ui/SettingsPage.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Identifiants internes inchangés depuis « Toupie Arena » : ils fixent le dossier
    // des toupies sauvegardées et des paramètres.
    QApplication::setApplicationName(QStringLiteral("ToupieArena"));
    QApplication::setOrganizationName(QStringLiteral("ToupieArena"));
    QApplication::setApplicationDisplayName(QStringLiteral("Top Arena"));
    QApplication::setApplicationVersion(QStringLiteral(TOUPIE_VERSION));

    // Avant toute page : catalogue, stadiums et coups sont traduits à leur création.
    toupie::Language::install(app);
    toupie::Theme::apply(app);
    toupie::SettingsPage::loadPreferences(); // Type de duel par défaut.

    toupie::MainWindow window;
    window.showFullScreen(); // F11 : fenêtré / plein écran.

    // Vérification d'intégration (CI) : l'application démarre puis se ferme.
    if (QCoreApplication::arguments().contains(QStringLiteral("--smoke-test")))
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);

    return app.exec();
}
