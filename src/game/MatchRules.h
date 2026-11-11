#pragma once

#include "game/BattleConfig.h"

#include <QJsonObject>
#include <QString>

namespace toupie {

/// Réglages de partie choisis par les joueurs (section « Personnalisation »).
///
/// Ce sont des multiplicateurs (1 = valeur normale) appliqués aux constantes
/// de combat, plus quelques valeurs absolues.
struct MatchRules
{
    double speed = 1.0;          ///< Vitesse de déplacement (lancer, propulsion, dash).
    double knockback = 1.0;      ///< Rebonds après un choc.
    double wear = 1.0;           ///< Usure naturelle de la rotation.
    double damage = 1.0;         ///< Dégâts des chocs et des coups spéciaux.
    double maxEndurance = 100.0; ///< Endurance de départ.
    double timeLimit = 0.0;      ///< Secondes ; 0 = aucune limite.
    double steering = 1.0;       ///< Pilotage aux flèches ; 0 = désactivé.
    bool specials = true;        ///< Coups spéciaux autorisés.
    double dashPower = 1.0;      ///< Bonus d'attaque des dashs.
    /// Type de duel. Par défaut, celui de la page Paramètres (voir setDefaultDuelMode) ;
    /// en réseau, les règles de l'hôte s'imposent aux clients.
    DuelMode duelMode = s_defaultDuelMode;

    static void setDefaultDuelMode(DuelMode mode) { s_defaultDuelMode = mode; }
    static DuelMode defaultDuelMode() { return s_defaultDuelMode; }

    static constexpr double kMinMultiplier = 0.0;
    static constexpr double kMaxMultiplier = 3.0;

    BattleConfig apply(const BattleConfig &base) const;
    bool isDefault() const;
    /// Résumé des réglages modifiés (« Vitesse 150 % · Sans limite de temps… »).
    QString summary() const;

    QJsonObject toJson() const;
    static MatchRules fromJson(const QJsonObject &json);

    bool operator==(const MatchRules &other) const;
    bool operator!=(const MatchRules &other) const { return !(*this == other); }

private:
    static inline DuelMode s_defaultDuelMode = DuelMode::Timing;
};

QString toString(DuelMode mode);

} // namespace toupie
