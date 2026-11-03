#pragma once

#include "game/Battle.h"

#include <QList>
#include <QObject>
#include <QStringList>

#include <vector>

namespace toupie {

enum class MatchPhase { Aiming, Countdown, Fighting, Finished };

/// Ce que l'arène a besoin de connaître pour afficher un combat, qu'il soit
/// simulé localement ou reçu du réseau.
struct MatchState
{
    Stadium stadium;
    BattleConfig config;            ///< Constantes effectives (stadium appliqué).
    StadiumGeometry geometry;
    std::vector<Spinner> spinners;
    QStringList names;
    QList<Vec2> startPositions;
    QList<bool> aimed;              ///< Joueurs ayant validé leur visée.

    MatchPhase phase = MatchPhase::Aiming;
    double countdown = 0.0;
    double elapsed = 0.0;
    int winner = -1;
    Battle::EndReason endReason = Battle::EndReason::None;
    QList<int> ranking;

    int playerCount() const { return static_cast<int>(spinners.size()); }
};

/// Un combat vu depuis un joueur : solo contre des bots, hôte réseau ou
/// client réseau. L'arène appelle update() à chaque image, transmet les
/// actions du joueur local et affiche state().
class MatchSession : public QObject
{
    Q_OBJECT

public:
    static constexpr double kCountdownSeconds = 3.0;

    explicit MatchSession(QObject *parent = nullptr);

    const MatchState &state() const { return m_state; }
    int localPlayer() const { return m_localPlayer; }
    bool localHasAimed() const { return m_state.aimed.value(m_localPlayer); }

    virtual void update(double dt) = 0;
    /// Valide l'angle de lancement du joueur local.
    virtual void submitAim(double angle) = 0;
    virtual void requestDash() = 0;

    /// Chocs survenus depuis le dernier appel (effets visuels).
    QList<ImpactEvent> takeImpacts();

signals:
    void phaseChanged();
    void finished(int winner);

protected:
    void setPhase(MatchPhase phase);
    /// Initialise l'état à partir d'un combat qui n'a pas encore commencé.
    void initFromBattle(const Battle &battle, const QStringList &names);
    /// Copie l'état dynamique du combat (positions, endurance, résultat).
    void syncFromBattle(Battle &battle);

    MatchState m_state;
    int m_localPlayer = 0;
    QList<ImpactEvent> m_impacts;
};

} // namespace toupie
