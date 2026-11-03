#pragma once

#include "core/Stats.h"
#include "game/Vec2.h"

#include <QList>

#include <QString>

#include <cstdint>
#include <random>

namespace toupie {

class Battle;

enum class Difficulty { Easy, Normal, Hard };
QString toString(Difficulty difficulty);

/// Adversaire contrôlé par l'ordinateur.
///
/// Le style de jeu dépend du type de la toupie pilotée :
/// - Attaque : vise l'adversaire au lancement et dashe dès qu'il est à portée ;
/// - Défense : reste près du centre et contre-attaque quand l'adversaire
///   arrive ou s'approche du bord ;
/// - Endurance : économise sa rotation et ne dashe que pour éjecter ;
/// - Équilibre : opportuniste, dashe quand il a l'avantage.
/// La difficulté règle le temps de réaction, la précision du lancer et
/// l'anticipation du mouvement adverse.
class BotController
{
public:
    BotController(int index, Difficulty difficulty, std::uint32_t seed = std::random_device{}());

    int index() const { return m_index; }
    Difficulty difficulty() const { return m_difficulty; }

    /// Angle de lancement (radians, repère écran).
    double chooseLaunchAngle(const Battle &battle);
    /// Force de lancement (entre launchPowerMin et launchPowerMax).
    double chooseLaunchPower(const Battle &battle);
    /// À appeler à chaque image pendant le combat.
    void update(Battle &battle, double dt);

private:
    /// Cible du prochain dash, -1 pour ne pas dasher.
    int chooseDashTarget(const Battle &battle);
    bool wantsToDashAt(const Battle &battle, int target, double *score);
    bool wantsSpecial(const Battle &battle);
    Vec2 chooseSteering(const Battle &battle);
    /// Direction d'esquive face à une vague qui arrive (nulle s'il n'y en a pas).
    Vec2 dodgeDirection(const Battle &battle);
    double reactionTime() const;
    double aimError() const;
    double dodgeSkill() const;

    int m_index;
    Difficulty m_difficulty;
    std::mt19937 m_rng;
    double m_decisionTimer = 0.0;
    double m_steerTimer = 0.0;
    QList<int> m_ignoredThreats; ///< Vagues que le bot n'a pas vues venir.
    QList<int> m_seenThreats;
};

} // namespace toupie
