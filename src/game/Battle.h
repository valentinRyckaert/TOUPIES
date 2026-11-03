#pragma once

#include "core/Top.h"
#include "game/BattleConfig.h"
#include "game/Vec2.h"

#include <QList>

#include <array>
#include <cstdint>
#include <random>

namespace toupie {

/// État d'une toupie pendant un combat.
struct Spinner
{
    enum class Status { Waiting, Spinning, RingOut, Stopped };

    Top top;
    Stats stats;
    int spinSign = 1;          ///< +1 horaire, -1 anti-horaire.

    Vec2 position;
    Vec2 velocity;
    double spin = 0.0;         ///< Endurance restante (0..maxSpin).
    double maxSpin = 100.0;
    double angle = 0.0;        ///< Angle de rotation propre (affichage).

    double dashTimer = 0.0;    ///< > 0 : dash en cours.
    double dashCooldown = 0.0; ///< > 0 : dash indisponible.
    double boostTimer = 0.0;   ///< > 0 : bonus d'attaque actif.
    Vec2 dashDirection;

    Status status = Status::Waiting;

    double spinRatio() const { return maxSpin > 0.0 ? spin / maxSpin : 0.0; }
    bool isActive() const { return status == Status::Spinning; }
    bool isDashing() const { return dashTimer > 0.0; }
    bool isBoosted() const { return boostTimer > 0.0; }
};

/// Un choc entre les deux toupies (utilisé pour les effets visuels).
struct ImpactEvent
{
    Vec2 point;
    double intensity = 0.0;
    std::array<double, 2> damage{};
};

/// Simulation d'un combat entre deux toupies.
///
/// Le moteur est indépendant de l'affichage : il avance par pas de temps
/// fixes, ce qui le rend déterministe pour une graine donnée.
class Battle
{
public:
    enum class Phase { Ready, Running, Finished };
    enum class EndReason { None, RingOut, SpinOut, Timeout };

    Battle(const Top &first, const Top &second, const BattleConfig &config = {},
           std::uint32_t seed = std::random_device{}());

    const BattleConfig &config() const { return m_config; }
    const Spinner &spinner(int index) const { return m_spinners.at(index); }
    Vec2 startPosition(int index) const;

    /// Lance les deux toupies simultanément (angles en radians, repère écran).
    void launch(double firstAngle, double secondAngle);

    bool canDash(int index) const;
    /// Déclenche un dash vers la toupie adverse si c'est possible.
    bool requestDash(int index);

    /// Fait avancer la simulation de dt secondes (découpées en pas fixes).
    void advance(double dt);

    Phase phase() const { return m_phase; }
    bool isFinished() const { return m_phase == Phase::Finished; }
    /// 0 ou 1, -1 pour un match nul.
    int winner() const { return m_winner; }
    EndReason endReason() const { return m_endReason; }
    double elapsed() const { return m_elapsed; }

    /// Récupère (et vide) la liste des chocs survenus depuis le dernier appel.
    QList<ImpactEvent> takeImpacts();

    /// Fonctions de formule exposées pour les tests et l'équilibrage.
    double passiveLossPerSecond(const Spinner &s) const;
    double effectiveAttack(const Spinner &s) const;
    double collisionDamage(const Spinner &attacker, const Spinner &target, double approachSpeed) const;
    double knockback(const Spinner &attacker, const Spinner &target, double approachSpeed) const;

private:
    void step(double h);
    void integrate(Spinner &s, double h);
    void resolveCollision();
    void checkEnd();
    void finish(int winner, EndReason reason);
    double mass(const Spinner &s) const;

    BattleConfig m_config;
    std::array<Spinner, 2> m_spinners;
    std::mt19937 m_rng;
    Phase m_phase = Phase::Ready;
    EndReason m_endReason = EndReason::None;
    int m_winner = -1;
    double m_elapsed = 0.0;
    double m_accumulator = 0.0;
    double m_contactCooldown = 0.0;
    QList<ImpactEvent> m_impacts;
};

} // namespace toupie
