#pragma once

#include "core/Stats.h"

#include <QString>

namespace toupie {

/// Les trois emplacements d'une toupie.
enum class PartKind { Wheel, Middle, Tip };

/// Une pièce de toupie (roue métallique, partie intermédiaire ou pointe).
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

    bool isValid() const { return !id.isEmpty(); }
};

inline QString toString(PartKind kind)
{
    switch (kind) {
    case PartKind::Wheel:
        return QStringLiteral("Roue métallique");
    case PartKind::Middle:
        return QStringLiteral("Partie intermédiaire");
    case PartKind::Tip:
        return QStringLiteral("Pointe");
    }
    return {};
}

} // namespace toupie
