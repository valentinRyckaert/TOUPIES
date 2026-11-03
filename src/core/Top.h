#pragma once

#include "core/Part.h"
#include "core/Stats.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace toupie {

class PartCatalog;

/// Une toupie : l'assemblage d'un boulon (coup spécial, sans statistiques),
/// d'une roue métallique, d'une partie intermédiaire et d'une pointe.
class Top
{
public:
    Top() = default;
    Top(QString name, Part wheel, Part middle, Part tip, Part bolt = {});

    const QString &name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    const Part &wheel() const { return m_wheel; }
    const Part &middle() const { return m_middle; }
    const Part &tip() const { return m_tip; }
    const Part &bolt() const { return m_bolt; }
    /// Identifiant du coup spécial (donné par le boulon).
    const QString &special() const { return m_bolt.special; }
    const Part &part(PartKind kind) const;
    void setPart(const Part &part);

    /// Somme des statistiques des pièces (le boulon n'en donne pas).
    Stats stats() const;
    TopType type() const { return classify(stats()); }
    /// Le sens de rotation est imposé par la roue métallique.
    SpinDirection spin() const { return m_wheel.spin; }

    bool isValid() const;

    /// Sérialisation : seuls le nom et les identifiants des pièces sont stockés.
    /// Une toupie sauvegardée sans boulon (ancienne version) reçoit le boulon
    /// par défaut du catalogue.
    QJsonObject toJson() const;
    static std::optional<Top> fromJson(const QJsonObject &json, const PartCatalog &catalog);

private:
    QString m_name;
    Part m_wheel;
    Part m_middle;
    Part m_tip;
    Part m_bolt;
};

} // namespace toupie
