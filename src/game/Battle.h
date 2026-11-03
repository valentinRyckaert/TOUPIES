#pragma once

#include "core/Top.h"
#include "game/BattleConfig.h"
#include "game/SpecialMove.h"
#include "game/Stadium.h"
#include "game/Vec2.h"

#include <QList>

#include <cstdint>
#include <random>
#include <vector>

namespace toupie {

/// État d'une toupie pendant un combat.
struct Spinner
{
    enum class Status { Waiting, Spinning, RingOut, Stopped, Forfeit };

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
    bool escaping = false;     ///< A franchi le rebord : sera éjectée.
    int lastHitBy = -1;        ///< Dernière toupie à l'avoir frappée.
    double lastHitTime = -1e9;

    Vec2 steer;                ///< Pilotage demandé (longueur <= 1).
    bool specialUsed = false;  ///< Coup spécial déjà lancé (une fois par partie).
    double slowTimer = 0.0;    ///< > 0 : ralentie (coup spécial « givre »).
    double slowStrength = 0.0;
    double disarmTimer = 0.0;  ///< > 0 : ni dash ni coup spécial.

    Status status = Status::Waiting;

    double spinRatio() const { return maxSpin > 0.0 ? spin / maxSpin : 0.0; }
    bool isActive() const { return status == Status::Spinning; }
    bool isDashing() const { return dashTimer > 0.0; }
    bool isBoosted() const { return boostTimer > 0.0; }
    bool isSlowed() const { return slowTimer > 0.0; }
    bool isDisarmed() const { return disarmTimer > 0.0; }
};

/// Vague d'un coup spécial en cours de propagation.
struct Projectile
{
    int id = 0;            ///< Identifiant unique dans le combat.
    QString move;          ///< Identifiant du SpecialMove.
    int owner = 0;
    bool ring = false;     ///< Anneau (s'élargit) ou vague (avance en ligne droite).
    Vec2 origin;
    Vec2 position;         ///< Vague : position du front.
    Vec2 direction;        ///< Vague : direction de propagation.
    double radius = 0.0;   ///< Anneau : rayon courant.
    double traveled = 0.0;
    double speed = 1.0;
    double range = 1.0;
    double width = 0.08;
    unsigned color = 0xffffff;
    QList<int> hit;        ///< Toupies déjà touchées.
};

/// Un choc entre deux toupies (utilisé pour les effets visuels).
struct ImpactEvent
{
    Vec2 point;
    double intensity = 0.0;
    int first = 0;
    int second = 1;
    bool special = false;  ///< Touché par un coup spécial.
    unsigned color = 0;    ///< Couleur du coup spécial.
};

/// Simulation d'un combat entre 2 et 4 toupies dans un stadium.
///
/// Le moteur est indépendant de l'affichage : il avance par pas de temps
/// fixes, ce qui le rend déterministe pour une graine donnée. La dernière
/// toupie en jeu l'emporte.
class Battle
{
public:
    enum class Phase { Ready, Running, Finished };
    enum class EndReason { None, RingOut, SpinOut, Timeout, Forfeit };

    static constexpr int kMinPlayers = 2;
    static constexpr int kMaxPlayers = 4;

    Battle(const QList<Top> &tops, const Stadium &stadium, const BattleConfig &config = {},
           std::uint32_t seed = std::random_device{}());
    /// Combat à deux dans le stadium classique.
    Battle(const Top &first, const Top &second, const BattleConfig &config = {},
           std::uint32_t seed = std::random_device{}());

    /// Constantes effectives (déjà modifiées par le stadium).
    const BattleConfig &config() const { return m_config; }
    const Stadium &stadium() const { return m_stadium; }
    const StadiumGeometry &geometry() const { return m_geometry; }

    int playerCount() const { return static_cast<int>(m_spinners.size()); }
    const Spinner &spinner(int index) const { return m_spinners.at(index); }
    const std::vector<Spinner> &spinners() const { return m_spinners; }
    Vec2 startPosition(int index) const;

    /// Lance toutes les toupies simultanément (angles en radians, repère
    /// écran ; forces entre launchPowerMin et launchPowerMax, 1 par défaut).
    void launch(const QList<double> &angles, const QList<double> &powers = {});
    void launch(double firstAngle, double secondAngle);

    bool canDash(int index) const;
    /// Déclenche un dash vers `target` (par défaut la toupie adverse la plus
    /// proche) si c'est possible.
    bool requestDash(int index, int target = -1);
    /// Adversaire en jeu le plus proche (-1 s'il n'y en a pas).
    int nearestOpponent(int index) const;

    /// Pilotage aux flèches : direction souhaitée (longueur <= 1, nulle = aucun).
    void setSteering(int index, Vec2 direction);

    bool canSpecial(int index) const;
    /// Lance le coup spécial du boulon vers l'adversaire le plus proche (une
    /// seule fois par partie, au prix de specialCost × l'endurance courante).
    bool requestSpecial(int index);
    const std::vector<Projectile> &projectiles() const { return m_projectiles; }

    /// Abandon (joueur déconnecté) : la toupie est éliminée.
    void forfeit(int index);

    /// Fait avancer la simulation de dt secondes (découpées en pas fixes).
    void advance(double dt);

    Phase phase() const { return m_phase; }
    bool isFinished() const { return m_phase == Phase::Finished; }
    /// Indice du vainqueur, -1 pour un match nul.
    int winner() const { return m_winner; }
    EndReason endReason() const { return m_endReason; }
    double elapsed() const { return m_elapsed; }
    /// Classement final (vainqueur en premier), disponible une fois le combat terminé.
    const QList<int> &ranking() const { return m_ranking; }

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
    void applyRim(Spinner &s);
    void resolveCollision(int i, int j);
    void updateProjectiles(double h);
    void applySpecialHit(const Projectile &projectile, const SpecialMove &move, int target);
    void eliminate(int index, Spinner::Status status);
    void checkEnd();
    void finish(int winner, EndReason reason);
    double mass(const Spinner &s) const;
    double stability(const Spinner &s) const;
    double &pairCooldown(int i, int j);

    Stadium m_stadium;
    BattleConfig m_config;
    StadiumGeometry m_geometry;
    std::vector<Spinner> m_spinners;
    std::vector<double> m_pairCooldowns;
    std::mt19937 m_rng;
    Phase m_phase = Phase::Ready;
    EndReason m_endReason = EndReason::None;
    int m_winner = -1;
    double m_elapsed = 0.0;
    double m_accumulator = 0.0;
    QList<int> m_eliminated;   ///< Ordre d'élimination.
    QList<int> m_ranking;
    QList<ImpactEvent> m_impacts;
    std::vector<Projectile> m_projectiles;
    int m_nextProjectileId = 1;
};

} // namespace toupie
