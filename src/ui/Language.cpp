#include "ui/Language.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace toupie::Language {

namespace {
const QString kSettingsKey = QStringLiteral("language");
const QString kSource = QStringLiteral("fr");
QTranslator *g_game = nullptr; ///< Traductions installées par install().
QTranslator *g_qt = nullptr;

void replace(QTranslator *&slot, QTranslator *translator)
{
    if (slot) {
        QCoreApplication::removeTranslator(slot);
        delete slot;
    }
    slot = translator;
    if (slot)
        QCoreApplication::installTranslator(slot);
}
} // namespace

QStringList available()
{
    return {kSource, QStringLiteral("en")};
}

QString nativeName(const QString &code)
{
    if (code == QLatin1String("en"))
        return QStringLiteral("English");
    return QStringLiteral("Français");
}

QString current()
{
    const QString saved = QSettings().value(kSettingsKey).toString();
    if (available().contains(saved))
        return saved;
    const QString system = QLocale::system().name().section(QLatin1Char('_'), 0, 0);
    return available().contains(system) ? system : QStringLiteral("en");
}

void setCurrent(const QString &code)
{
    if (available().contains(code))
        QSettings().setValue(kSettingsKey, code);
}

void install(QCoreApplication &app)
{
    const QString code = current();
    // Textes du jeu (embarqués) ; la langue source n'a pas de fichier.
    auto *game = new QTranslator(&app);
    if (code == kSource || !game->load(QStringLiteral(":/i18n/toparena_%1.qm").arg(code))) {
        delete game;
        game = nullptr;
    }
    replace(g_game, game);
    // Boutons standard de Qt (Oui, Non, Fermer…) quand la traduction est installée.
    auto *qt = new QTranslator(&app);
    if (!qt->load(QStringLiteral("qtbase_%1").arg(code),
                  QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        delete qt;
        qt = nullptr;
    }
    replace(g_qt, qt);
}

} // namespace toupie::Language
