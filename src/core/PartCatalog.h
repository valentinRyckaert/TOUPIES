#pragma once

#include "core/Part.h"
#include "core/Top.h"

#include <QList>
#include <QString>

#include <optional>

namespace toupie {

/// Catalogue des pièces disponibles et des toupies pré-assemblées.
///
/// Règle d'équilibrage : toutes les pièces d'un même emplacement disposent
/// du même « budget » de points (roue : 10, intermédiaire : 6, pointe : 7).
/// Toutes les toupies totalisent donc 23 points : assembler une toupie
/// consiste à répartir ces points, jamais à en gagner.
class PartCatalog
{
public:
    static constexpr int kWheelBudget = 10;
    static constexpr int kMiddleBudget = 6;
    static constexpr int kTipBudget = 7;

    /// Catalogue par défaut du jeu.
    static const PartCatalog &instance();

    PartCatalog() = default;
    explicit PartCatalog(QList<Part> parts);

    const QList<Part> &parts() const { return m_parts; }
    QList<Part> parts(PartKind kind) const;
    std::optional<Part> find(const QString &id) const;

    /// Toupies pré-assemblées (une par type).
    QList<Top> presets() const;

    static int budget(PartKind kind);

private:
    QList<Part> m_parts;
};

} // namespace toupie
