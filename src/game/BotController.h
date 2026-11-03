#pragma once

#include "core/Stats.h"

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
    /// À appeler à chaque image pendant le combat.
    void update(Battle &battle, double dt);

private:
    /// Cible du prochain dash, -1 pour ne pas dasher.
    int chooseDashTarget(const Battle &battle);
    bool wantsToDashAt(const Battle &battle, int target, double *score);
    double reactionTime() const;
    double aimError() const;

    int m_index;
    Difficulty m_difficulty;
    std::mt19937 m_rng;
    double m_decisionTimer = 0.0;
};

} // namespace toupie
