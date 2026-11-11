#pragma once

#include "core/Stats.h"
#include "game/Vec2.h"

#include <QList>
#include <QString>

#include <cstdint>
#include <random>

namespace toupie {

class Battle;
struct Spinner;
struct SpecialMove;

enum class Difficulty { Easy, Normal, Hard };
QString toString(Difficulty difficulty);

/// Adversaire contrôlé par l'ordinateur.
///
/// À chaque décision, le bot analyse la partie (adversaires, course à
/// l'usure, position par rapport au bord) et choisit un plan adapté à sa
/// toupie :
/// - Chasser : aller au contact ; en difficile, se placer côté centre de la
///   cible pour la pousser vers le bord et ne dasher que bien aligné ;
/// - Tenir : garder le centre et contrer ;
/// - Esquiver : garder ses distances en restant loin du bord, pour gagner à
///   l'usure.
/// Le coup spécial (une fois par partie, coûte un quart de l'endurance) est
/// lancé quand sa valeur estimée (dégâts, éjection probable, coup fatal,
/// effet utile pour la suite) dépasse son coût réel ; en difficile, le bot
/// enchaîne des combos (givre ou distorsion puis dash, onde sur une cible au
/// bord…).
/// La difficulté règle les réflexes, la précision, l'esquive des vagues, le
/// timing en duel et le niveau tactique.
class BotController
{
public:
    enum class Plan { Hunt, Hold, Evade };

    BotController(int index, Difficulty difficulty, std::uint32_t seed = std::random_device{}());

    int index() const { return m_index; }
    Difficulty difficulty() const { return m_difficulty; }
    Plan plan() const { return m_plan; }

    /// Angle de lancement (radians, repère écran).
    double chooseLaunchAngle(const Battle &battle);
    /// Force de lancement (entre launchPowerMin et launchPowerMax).
    double chooseLaunchPower(const Battle &battle);
    /// À appeler à chaque image pendant le combat.
    void update(Battle &battle, double dt);

    /// Valeur estimée (en points d'endurance) du lancer du coup spécial
    /// maintenant ; négative si le coût dépasse le bénéfice attendu.
    double specialValue(const Battle &battle) const;

    /// Autorise ou non le bot à lancer son coup spécial (mesures, handicaps).
    void setSpecialsAllowed(bool allowed) { m_specialsAllowed = allowed; }

private:
    struct Skill
    {
        double reaction;    ///< Secondes entre deux décisions.
        double aimError;    ///< Écart-type de l'erreur de visée (radians).
        double dodgeWaves;  ///< Probabilité de remarquer une vague.
        double rangeFactor; ///< Appréciation de la portée du dash (>1 : dash trop tôt).
        double hesitate;    ///< Probabilité de laisser passer une occasion.
        bool tactics;       ///< Placement et choix de plan élaborés.
        bool combos;        ///< Enchaînements coup spécial + dash.
        bool anticipate;    ///< Prévision des déplacements adverses.
        double duelTiming;  ///< Écart-type du timing en duel (secondes).
        double rushRate;    ///< Duel en rafale : appuis par seconde.
    };
    static Skill skillFor(Difficulty difficulty);

    /// Rapport entre mon temps restant à l'usure et celui de l'adversaire le
    /// plus endurant (> 1 : je gagne la course).
    double enduranceRace(const Battle &battle) const;
    int chooseTarget(const Battle &battle) const;
    Plan choosePlan(const Battle &battle, int target) const;
    Vec2 chooseSteering(const Battle &battle, int target);
    Vec2 dodgeDirection(const Battle &battle);
    int chooseDashTarget(const Battle &battle);
    bool wantsToDashAt(const Battle &battle, int target, double *score);
    /// Valeur estimée d'un dash vers `target` (points d'endurance nets).
    double dashValue(const Battle &battle, int target) const;
    double hitProbability(const Battle &battle, const SpecialMove &move, int target) const;

    int m_index;
    Difficulty m_difficulty;
    Skill m_skill;
    mutable std::mt19937 m_rng;
    Plan m_plan = Plan::Hold;
    bool m_specialsAllowed = true;
    double m_decisionTimer = 0.0;
    double m_steerTimer = 0.0;
    double m_rushTimer = 0.0;
    QList<int> m_ignoredThreats; ///< Vagues que le bot n'a pas vues venir.
    QList<int> m_seenThreats;
};

} // namespace toupie
