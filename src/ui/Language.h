#pragma once

#include <QString>
#include <QStringList>

class QCoreApplication;

namespace toupie::Language {

/// Langues proposées (codes ISO 639-1) ; le français est la langue source.
QStringList available();
/// Nom de la langue dans cette langue (« Français », « English »), jamais traduit.
QString nativeName(const QString &code);

/// Langue enregistrée dans les paramètres, à défaut celle du système (anglais
/// si elle n'est pas proposée).
QString current();
/// Enregistre la langue ; elle s'applique au prochain démarrage (install()).
void setCurrent(const QString &code);

/// Charge les traductions de la langue courante (jeu et boîtes de dialogue Qt),
/// en remplaçant celles d'un appel précédent.
/// À appeler avant de créer la moindre page : le catalogue de pièces, les
/// stadiums et les coups spéciaux sont traduits à leur création.
void install(QCoreApplication &app);

} // namespace toupie::Language
