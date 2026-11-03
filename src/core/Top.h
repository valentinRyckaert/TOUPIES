#pragma once

#include "core/Part.h"
#include "core/Stats.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace toupie {

class PartCatalog;

/// Une toupie : l'assemblage d'une roue métallique, d'une partie
/// intermédiaire et d'une pointe.
class Top
{
public:
    Top() = default;
    Top(QString name, Part wheel, Part middle, Part tip);

    const QString &name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    const Part &wheel() const { return m_wheel; }
    const Part &middle() const { return m_middle; }
    const Part &tip() const { return m_tip; }
    const Part &part(PartKind kind) const;
    void setPart(const Part &part);

    /// Somme des statistiques des trois pièces.
    Stats stats() const;
    TopType type() const { return classify(stats()); }
    /// Le sens de rotation est imposé par la roue métallique.
    SpinDirection spin() const { return m_wheel.spin; }

    bool isValid() const;

    /// Sérialisation : seuls le nom et les identifiants des pièces sont stockés.
    QJsonObject toJson() const;
    static std::optional<Top> fromJson(const QJsonObject &json, const PartCatalog &catalog);

private:
    QString m_name;
    Part m_wheel;
    Part m_middle;
    Part m_tip;
};

} // namespace toupie
