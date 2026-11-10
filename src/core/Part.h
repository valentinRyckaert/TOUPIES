#pragma once

#include "core/Stats.h"

#include <QCoreApplication>
#include <QString>

namespace toupie {

/// Les quatre emplacements d'une toupie, de haut en bas.
enum class PartKind { Wheel, Middle, Tip, Bolt };

/// Une pièce de toupie (boulon, roue métallique, partie intermédiaire ou pointe).
struct Part
{
    QString id;          ///< Identifiant stable, utilisé pour la sauvegarde.
    QString name;        ///< Nom affiché.
    PartKind kind = PartKind::Wheel;
    Stats stats;
    /// Sens de rotation : n'a de sens que pour une roue métallique.
    SpinDirection spin = SpinDirection::Clockwise;
    /// Teinte (0-359) utilisée pour dessiner la pièce.
    int hue = 0;
    /// Boulon uniquement : identifiant du coup spécial débloqué (voir SpecialMove).
    QString special;

    bool isValid() const { return !id.isEmpty(); }
};

inline QString toString(PartKind kind)
{
    switch (kind) {
    case PartKind::Wheel:
        return QCoreApplication::translate("toupie::Part", "Roue métallique");
    case PartKind::Middle:
        return QCoreApplication::translate("toupie::Part", "Partie intermédiaire");
    case PartKind::Tip:
        return QCoreApplication::translate("toupie::Part", "Pointe");
    case PartKind::Bolt:
        return QCoreApplication::translate("toupie::Part", "Boulon");
    }
    return {};
}

} // namespace toupie
