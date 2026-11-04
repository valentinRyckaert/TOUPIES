#pragma once

#include "game/Tournament.h"
#include "session/MatchSession.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <array>
#include <memory>
#include <random>
#include <vector>

namespace toupie::net {

class LobbyClient;
class LobbyServer;

/// Un tournoi vu depuis un joueur. Le directeur (solo ou hôte réseau) le fait
/// tourner ; le suiveur (client réseau) reflète l'état diffusé par l'hôte.
class TournamentController : public QObject
{
    Q_OBJECT

public:
    /// Match humain du tour courant, en préparation ou en cours.
    struct LiveMatch
    {
        int match = -1;
        MatchSeries series;
        std::array<bool, 2> ready{false, false}; ///< Ordre des toupies validé.
    };

    const Tournament &tournament() const { return m_tournament; }
    const QList<LiveMatch> &live() const { return m_live; }
    const LiveMatch *liveOf(int match) const;
    int localParticipant() const { return m_tournament.participantOf(m_localPlayerId); }
    /// Match du tour courant que le joueur local doit encore jouer (-1 : aucun).
    int localMatch() const;
    /// Le joueur local a validé l'ordre de ses toupies pour ce match.
    bool localReady() const;

    /// Prêt à jouer, avec l'ordre de passage de ses toupies (indices).
    virtual void submitOrder(const QList<int> &order) = 0;
    /// Abandon du match en cours (l'adversaire le remporte).
    virtual void forfeitLocal() = 0;

    /// Session de la nouvelle manche du joueur local (après sessionReady()).
    std::unique_ptr<MatchSession> takeSession() { return std::move(m_session); }

signals:
    void changed();
    void sessionReady();
    void localMatchOver();

protected:
    TournamentController(int localPlayerId, QObject *parent);

    Tournament m_tournament;
    QList<LiveMatch> m_live;
    int m_localPlayerId = 0;
    std::unique_ptr<MatchSession> m_session;
};

/// Fait tourner un tournoi : simule les matchs entre bots, joue les manches
/// des matchs humains (HostSession, bots compris) et enchaîne les tours. Avec
/// un serveur, diffuse l'état aux clients et lance leurs manches ; plusieurs
/// matchs humains d'un même tour se jouent en parallèle.
class TournamentDirector : public TournamentController
{
    Q_OBJECT

public:
    /// Pause entre deux manches et après la dernière (résultat affiché).
    static constexpr int kPauseMs = 3500;

    /// `server` nul : tournoi solo, le joueur local a l'identifiant 0.
    TournamentDirector(LobbyServer *server, Tournament tournament, std::uint32_t seed,
                       QObject *parent = nullptr);
    ~TournamentDirector() override;

    void submitOrder(const QList<int> &order) override;
    void forfeitLocal() override;

private:
    struct Match
    {
        int id = 0;            ///< Identifiant unique (les indices changent à chaque tour).
        int index = -1;        ///< Indice dans le tour courant.
        MatchSeries series;
        std::array<bool, 2> ready{false, false};
        std::unique_ptr<MatchSession> owned; ///< Manche simulée ici (sinon affichée par l'interface).
        QPointer<MatchSession> session;
    };

    void startRound();
    void setReady(int participant, const QList<int> &order);
    void forfeit(int participant);
    void playNextBattle(Match &match);
    void onBattleFinished(int id);
    void finishMatch(int id);
    void scheduleNextRound();
    Match *find(int id);
    Match *findByParticipant(int participant);
    void tick();
    void publish();
    void onGameMessage(int playerId, const QJsonObject &message);
    void onPlayerLeft(int playerId);

    QPointer<LobbyServer> m_server;
    std::vector<std::unique_ptr<Match>> m_matches;
    std::mt19937 m_rng;
    QTimer m_timer;
    QElapsedTimer m_clock;
    int m_nextId = 1;
    bool m_advancing = false;
};

/// Client réseau : suit le tournoi de l'hôte et joue ses manches.
class TournamentFollower : public TournamentController
{
    Q_OBJECT

public:
    TournamentFollower(LobbyClient *client, QObject *parent = nullptr);

    /// Applique un message « tournament » de l'hôte.
    void apply(const QJsonObject &message);

    void submitOrder(const QList<int> &order) override;
    void forfeitLocal() override;

private:
    QPointer<LobbyClient> m_client;
    int m_playing = -1; ///< Match en cours du joueur local.
};

} // namespace toupie::net
