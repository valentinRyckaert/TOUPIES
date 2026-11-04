#pragma once

#include "core/Top.h"
#include "game/Battle.h"
#include "game/BotController.h"
#include "game/MatchRules.h"

#include <QJsonObject>
#include <QList>
#include <QString>

#include <array>
#include <cstdint>
#include <optional>

namespace toupie {

class PartCatalog;

enum class TournamentFormat { League, Knockout };
/// Express : une toupie, premier à 2 manches gagnées. Classique : trois
/// toupies jouées dans l'ordre choisi, premier à 4 points (arrêt 1 pt,
/// éjection 2 pts).
enum class MatchFormat { Express, Classic };
QString toString(TournamentFormat format);
QString toString(MatchFormat format);

/// Score d'un match de tournoi, joué en plusieurs manches.
struct MatchSeries
{
    MatchFormat format = MatchFormat::Express;
    std::array<int, 2> score{0, 0};
    int rounds = 0;                   ///< Manches jouées (nuls compris).
    std::array<QList<int>, 2> order;  ///< Ordre de passage des toupies de chaque camp.

    static int topsFor(MatchFormat format) { return format == MatchFormat::Classic ? 3 : 1; }
    int target() const { return format == MatchFormat::Classic ? 4 : 2; }
    /// Points marqués par le vainqueur d'une manche terminée ainsi.
    int pointsFor(Battle::EndReason reason) const;
    /// Enregistre une manche (`winnerSide` 0 ou 1, -1 pour un nul).
    void recordRound(int winnerSide, Battle::EndReason reason);
    /// Abandon du camp `side` : l'adversaire gagne le match.
    void forfeit(int side);
    bool isOver() const { return winner() >= 0; }
    int winner() const;
    /// Indice (dans les toupies du participant) de la toupie jouée par `side`
    /// à la prochaine manche.
    int topFor(int side) const;

    QJsonObject toJson() const;
    static MatchSeries fromJson(const QJsonObject &json);
};

/// Participant : joueur humain (identifiant réseau, 0 pour l'hôte ou le
/// joueur solo) ou bot.
struct Participant
{
    QString name;
    int playerId = -1;   ///< -1 : bot.
    Difficulty level = Difficulty::Normal;
    QList<Top> tops;     ///< 1 (express) ou 3 (classique).

    bool isBot() const { return playerId < 0; }
};

struct TournamentMatch
{
    int a = -1;
    int b = -1;          ///< -1 : exempt (a passe le tour sans jouer).
    std::array<int, 2> score{0, 0};
    bool played = false;

    bool isBye() const { return b < 0; }
    /// Participant vainqueur (-1 si non joué ou égalité).
    int winner() const;
};

struct TournamentSettings
{
    TournamentFormat format = TournamentFormat::League;
    MatchFormat matchFormat = MatchFormat::Express;
    int participants = 8;
    QList<Difficulty> bots;   ///< Niveau de chaque bot (participants - humains).
    QString stadiumId;
    MatchRules rules;

    /// Ajuste le nombre de bots pour `humans` joueurs humains (nouveaux bots : normal).
    void fitBots(int humans);

    QJsonObject toJson() const;
    static TournamentSettings fromJson(const QJsonObject &json);
    bool operator==(const TournamentSettings &other) const;
    bool operator!=(const TournamentSettings &other) const { return !(*this == other); }
};

struct Standing
{
    int participant = -1;
    int played = 0;
    int won = 0;
    int lost = 0;
    int points = 0;      ///< Manches (express) ou points (classique) gagnés.
    int against = 0;
};

/// Tournoi : ligue (chacun rencontre chacun une fois, le classement additionne
/// les manches ou points gagnés par chaque joueur) ou élimination directe
/// (tableau avec exemptions si le nombre de participants n'est pas une
/// puissance de 2).
class Tournament
{
public:
    static constexpr int kMinParticipants = 4;
    static constexpr int kMaxParticipants = 32;
    static constexpr int kMaxHumans = 4;

    Tournament() = default;
    /// Les participants sont mélangés (tirage au sort) avec `seed`.
    Tournament(const TournamentSettings &settings, QList<Participant> participants,
               std::uint32_t seed);

    const TournamentSettings &settings() const { return m_settings; }
    const QList<Participant> &participants() const { return m_participants; }
    Participant &participant(int index) { return m_participants[index]; }
    const QList<QList<TournamentMatch>> &rounds() const { return m_rounds; }
    int currentRound() const { return m_current; }
    /// Nombre total de tours (connu dès le départ dans les deux formats).
    int roundCount() const;
    QString roundName(int round) const;

    /// Participant d'un joueur humain (-1 s'il n'en a pas).
    int participantOf(int playerId) const;
    /// Match du tour courant de `participant` (-1 : aucun).
    int matchOf(int participant) const;

    void record(int match, const MatchSeries &series);
    bool roundComplete() const;
    /// Passe au tour suivant (une fois le tour courant terminé).
    void nextRound();
    bool isFinished() const;
    int champion() const;
    /// Classement (points, victoires, différence), du premier au dernier.
    QList<Standing> standings() const;

    QJsonObject toJson() const;
    static std::optional<Tournament> fromJson(const QJsonObject &json, const PartCatalog &catalog);

private:
    TournamentSettings m_settings;
    QList<Participant> m_participants;
    QList<QList<TournamentMatch>> m_rounds;
    int m_current = 0;
};

/// Joue entièrement (sans affichage) un match de tournoi entre deux bots.
MatchSeries playBotMatch(const Participant &a, const Participant &b, MatchFormat format,
                         const Stadium &stadium, const MatchRules &rules, std::uint32_t seed);

/// Ordre de passage aléatoire des `count` toupies d'un bot.
QList<int> randomOrder(int count, std::uint32_t seed);

/// Complète les joueurs humains avec les bots des réglages (toupies
/// différentes tirées au hasard dans `pool`).
QList<Participant> addBots(QList<Participant> humans, const TournamentSettings &settings,
                           const QList<Top> &pool, std::uint32_t seed);

} // namespace toupie
