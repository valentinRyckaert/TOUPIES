#include "game/Tournament.h"

#include "core/PartCatalog.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <algorithm>
#include <numeric>
#include <random>

namespace toupie {

namespace {
/// Garde-fou contre une suite improbable de manches nulles.
constexpr int kMaxRounds = 30;

int nextPowerOfTwo(int n)
{
    int size = 1;
    while (size < n)
        size *= 2;
    return size;
}
} // namespace

QString toString(TournamentFormat format)
{
    return format == TournamentFormat::League ? QCoreApplication::translate("toupie::Tournament", "Ligue")
                                              : QCoreApplication::translate("toupie::Tournament", "Élimination directe");
}

QString toString(MatchFormat format)
{
    return format == MatchFormat::Classic ? QCoreApplication::translate("toupie::Tournament", "Classique")
                                          : QCoreApplication::translate("toupie::Tournament", "Express");
}

// ---------------------------------------------------------------------------
// MatchSeries
// ---------------------------------------------------------------------------

int MatchSeries::pointsFor(Battle::EndReason reason) const
{
    if (reason == Battle::EndReason::Forfeit)
        return target();
    if (format == MatchFormat::Express)
        return 1;
    return reason == Battle::EndReason::RingOut ? 2 : 1;
}

void MatchSeries::recordRound(int winnerSide, Battle::EndReason reason)
{
    if (isOver())
        return;
    ++rounds;
    if (winnerSide == 0 || winnerSide == 1)
        score[winnerSide] += pointsFor(reason);
    if (rounds >= kMaxRounds && !isOver())
        score[score[1] > score[0] ? 1 : 0] = target();
}

void MatchSeries::forfeit(int side)
{
    const int other = side == 0 ? 1 : 0;
    score[other] = std::max(score[other], target());
}

int MatchSeries::winner() const
{
    if (score[0] >= target())
        return 0;
    return score[1] >= target() ? 1 : -1;
}

int MatchSeries::topFor(int side) const
{
    const QList<int> &o = order[side];
    return o.isEmpty() ? 0 : o.at(rounds % o.size());
}

QJsonObject MatchSeries::toJson() const
{
    auto list = [](const QList<int> &values) {
        QJsonArray array;
        for (int v : values)
            array.append(v);
        return array;
    };
    return {{"format", static_cast<int>(format)},
            {"score", QJsonArray{score[0], score[1]}},
            {"rounds", rounds},
            {"order", QJsonArray{list(order[0]), list(order[1])}}};
}

MatchSeries MatchSeries::fromJson(const QJsonObject &json)
{
    MatchSeries s;
    s.format = json.value("format").toInt() == 1 ? MatchFormat::Classic : MatchFormat::Express;
    const QJsonArray score = json.value("score").toArray();
    s.score = {score.at(0).toInt(), score.at(1).toInt()};
    s.rounds = std::max(0, json.value("rounds").toInt());
    const QJsonArray order = json.value("order").toArray();
    for (int side = 0; side < 2; ++side) {
        for (const QJsonValue &v : order.at(side).toArray())
            s.order[side].append(std::clamp(v.toInt(), 0, topsFor(s.format) - 1));
    }
    return s;
}

// ---------------------------------------------------------------------------
// Réglages et matchs
// ---------------------------------------------------------------------------

int TournamentMatch::winner() const
{
    if (!played)
        return -1;
    if (isBye())
        return a;
    if (score[0] == score[1])
        return -1;
    return score[0] > score[1] ? a : b;
}

void TournamentSettings::fitBots(int humans)
{
    const int count = std::max(0, participants - humans);
    while (bots.size() > count)
        bots.removeLast();
    while (bots.size() < count)
        bots.append(Difficulty::Normal);
}

QJsonObject TournamentSettings::toJson() const
{
    QJsonArray levels;
    for (Difficulty d : bots)
        levels.append(static_cast<int>(d));
    return {{"format", static_cast<int>(format)},
            {"match", static_cast<int>(matchFormat)},
            {"participants", participants},
            {"bots", levels},
            {"stadium", stadiumId},
            {"rules", rules.toJson()}};
}

TournamentSettings TournamentSettings::fromJson(const QJsonObject &json)
{
    TournamentSettings s;
    s.format = json.value("format").toInt() == 1 ? TournamentFormat::Knockout
                                                 : TournamentFormat::League;
    s.matchFormat = json.value("match").toInt() == 1 ? MatchFormat::Classic
                                                     : MatchFormat::Express;
    s.participants = std::clamp(json.value("participants").toInt(8), Tournament::kMinParticipants,
                                Tournament::kMaxParticipants);
    for (const QJsonValue &v : json.value("bots").toArray()) {
        if (s.bots.size() < s.participants)
            s.bots.append(static_cast<Difficulty>(std::clamp(v.toInt(1), 0, 2)));
    }
    s.stadiumId = json.value("stadium").toString();
    s.rules = MatchRules::fromJson(json.value("rules").toObject());
    return s;
}

bool TournamentSettings::operator==(const TournamentSettings &o) const
{
    return format == o.format && matchFormat == o.matchFormat && participants == o.participants
        && bots == o.bots && stadiumId == o.stadiumId && rules == o.rules;
}

// ---------------------------------------------------------------------------
// Tournoi
// ---------------------------------------------------------------------------

Tournament::Tournament(const TournamentSettings &settings, QList<Participant> participants,
                       std::uint32_t seed)
    : m_settings(settings)
    , m_participants(std::move(participants))
{
    std::mt19937 rng(seed);
    QList<int> order(m_participants.size());
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);

    if (m_settings.format == TournamentFormat::League) {
        // Méthode du cercle : le premier reste fixe, les autres tournent.
        if (order.size() % 2 != 0)
            order.append(-1);
        const int n = static_cast<int>(order.size());
        for (int r = 0; r < n - 1; ++r) {
            QList<TournamentMatch> round;
            for (int i = 0; i < n / 2; ++i) {
                TournamentMatch m;
                m.a = order.at(i);
                m.b = order.at(n - 1 - i);
                if (m.a < 0)
                    std::swap(m.a, m.b);
                m.played = m.isBye();
                round.append(m);
            }
            m_rounds.append(round);
            order.insert(1, order.takeLast());
        }
    } else {
        // Tableau : les places vides (exemptions) sont en fin de tirage et
        // tombent donc face à des participants réels.
        const int size = nextPowerOfTwo(static_cast<int>(order.size()));
        while (order.size() < size)
            order.append(-1);
        QList<TournamentMatch> round;
        for (int i = 0; i < size / 2; ++i) {
            TournamentMatch m;
            m.a = order.at(i);
            m.b = order.at(size - 1 - i);
            m.played = m.isBye();
            round.append(m);
        }
        m_rounds.append(round);
    }
}

int Tournament::roundCount() const
{
    const int n = static_cast<int>(m_participants.size());
    if (m_settings.format == TournamentFormat::League)
        return n % 2 == 0 ? n - 1 : n;
    int rounds = 0;
    for (int size = nextPowerOfTwo(n); size > 1; size /= 2)
        ++rounds;
    return rounds;
}

QString Tournament::roundName(int round) const
{
    if (m_settings.format == TournamentFormat::League)
        return QCoreApplication::translate("toupie::Tournament", "Journée %1/%2").arg(round + 1).arg(roundCount());
    switch (roundCount() - round) {
    case 1:
        return QCoreApplication::translate("toupie::Tournament", "Finale");
    case 2:
        return QCoreApplication::translate("toupie::Tournament", "Demi-finales");
    case 3:
        return QCoreApplication::translate("toupie::Tournament", "Quarts de finale");
    case 4:
        return QCoreApplication::translate("toupie::Tournament", "Huitièmes de finale");
    default:
        return QCoreApplication::translate("toupie::Tournament", "Seizièmes de finale");
    }
}

int Tournament::participantOf(int playerId) const
{
    for (int i = 0; i < m_participants.size(); ++i) {
        if (playerId >= 0 && m_participants.at(i).playerId == playerId)
            return i;
    }
    return -1;
}

int Tournament::matchOf(int participant) const
{
    if (participant < 0 || m_current >= m_rounds.size())
        return -1;
    const QList<TournamentMatch> &round = m_rounds.at(m_current);
    for (int i = 0; i < round.size(); ++i) {
        if (!round.at(i).isBye() && (round.at(i).a == participant || round.at(i).b == participant))
            return i;
    }
    return -1;
}

void Tournament::record(int match, const MatchSeries &series)
{
    if (m_current >= m_rounds.size() || match < 0 || match >= m_rounds[m_current].size())
        return;
    TournamentMatch &m = m_rounds[m_current][match];
    m.score = series.score;
    m.played = true;
}

bool Tournament::roundComplete() const
{
    if (m_current >= m_rounds.size())
        return false;
    const QList<TournamentMatch> &round = m_rounds.at(m_current);
    return std::all_of(round.begin(), round.end(),
                       [](const TournamentMatch &m) { return m.played; });
}

void Tournament::nextRound()
{
    if (!roundComplete() || isFinished())
        return;
    if (m_settings.format == TournamentFormat::Knockout) {
        const QList<TournamentMatch> &previous = m_rounds.at(m_current);
        QList<TournamentMatch> round;
        for (int i = 0; i + 1 < previous.size(); i += 2) {
            TournamentMatch m;
            m.a = previous.at(i).winner();
            m.b = previous.at(i + 1).winner();
            round.append(m);
        }
        m_rounds.append(round);
    }
    ++m_current;
}

bool Tournament::isFinished() const
{
    return m_current == roundCount() - 1 && roundComplete();
}

int Tournament::champion() const
{
    if (!isFinished())
        return -1;
    if (m_settings.format == TournamentFormat::Knockout)
        return m_rounds.last().first().winner();
    return standings().first().participant;
}

QList<Standing> Tournament::standings() const
{
    QList<Standing> table(m_participants.size());
    for (int i = 0; i < table.size(); ++i)
        table[i].participant = i;
    for (const QList<TournamentMatch> &round : m_rounds) {
        for (const TournamentMatch &m : round) {
            if (!m.played || m.isBye())
                continue;
            const int winner = m.winner();
            for (int side = 0; side < 2; ++side) {
                Standing &s = table[side == 0 ? m.a : m.b];
                ++s.played;
                s.points += m.score[side];
                s.against += m.score[1 - side];
                if (winner >= 0)
                    (winner == s.participant ? s.won : s.lost)++;
            }
        }
    }
    std::stable_sort(table.begin(), table.end(), [](const Standing &x, const Standing &y) {
        if (x.points != y.points)
            return x.points > y.points;
        if (x.won != y.won)
            return x.won > y.won;
        return x.points - x.against > y.points - y.against;
    });
    return table;
}

QJsonObject Tournament::toJson() const
{
    QJsonArray participants;
    for (const Participant &p : m_participants) {
        QJsonArray tops;
        for (const Top &top : p.tops)
            tops.append(top.toJson());
        participants.append(QJsonObject{{"name", p.name},
                                        {"id", p.playerId},
                                        {"level", static_cast<int>(p.level)},
                                        {"tops", tops}});
    }
    QJsonArray rounds;
    for (const QList<TournamentMatch> &round : m_rounds) {
        QJsonArray matches;
        for (const TournamentMatch &m : round)
            matches.append(QJsonArray{m.a, m.b, m.score[0], m.score[1], m.played ? 1 : 0});
        rounds.append(matches);
    }
    return {{"settings", m_settings.toJson()},
            {"participants", participants},
            {"rounds", rounds},
            {"current", m_current}};
}

std::optional<Tournament> Tournament::fromJson(const QJsonObject &json, const PartCatalog &catalog)
{
    Tournament t;
    t.m_settings = TournamentSettings::fromJson(json.value("settings").toObject());
    for (const QJsonValue &value : json.value("participants").toArray()) {
        const QJsonObject o = value.toObject();
        Participant p;
        p.name = o.value("name").toString();
        p.playerId = o.value("id").toInt(-1);
        p.level = static_cast<Difficulty>(std::clamp(o.value("level").toInt(1), 0, 2));
        for (const QJsonValue &top : o.value("tops").toArray()) {
            const auto parsed = Top::fromJson(top.toObject(), catalog);
            if (!parsed)
                return std::nullopt;
            p.tops.append(*parsed);
        }
        if (p.tops.isEmpty())
            return std::nullopt;
        t.m_participants.append(p);
    }
    const int count = static_cast<int>(t.m_participants.size());
    if (count < kMinParticipants || count > kMaxParticipants)
        return std::nullopt;
    for (const QJsonValue &value : json.value("rounds").toArray()) {
        QList<TournamentMatch> round;
        for (const QJsonValue &mv : value.toArray()) {
            const QJsonArray a = mv.toArray();
            TournamentMatch m;
            m.a = a.at(0).toInt(-1);
            m.b = a.at(1).toInt(-1);
            if (m.a < 0 || m.a >= count || m.b >= count)
                return std::nullopt;
            m.score = {a.at(2).toInt(), a.at(3).toInt()};
            m.played = a.at(4).toInt() != 0;
            round.append(m);
        }
        t.m_rounds.append(round);
    }
    t.m_current = std::clamp(json.value("current").toInt(), 0,
                             std::max(0, static_cast<int>(t.m_rounds.size()) - 1));
    if (t.m_rounds.isEmpty())
        return std::nullopt;
    return t;
}

// ---------------------------------------------------------------------------
// Matchs entre bots
// ---------------------------------------------------------------------------

QList<int> randomOrder(int count, std::uint32_t seed)
{
    QList<int> order(count);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(seed);
    std::shuffle(order.begin(), order.end(), rng);
    return order;
}

QList<Participant> addBots(QList<Participant> humans, const TournamentSettings &settings,
                           const QList<Top> &pool, std::uint32_t seed)
{
    std::mt19937 rng(seed);
    const int tops = MatchSeries::topsFor(settings.matchFormat);
    for (int i = 0; i < settings.bots.size(); ++i) {
        Participant bot;
        bot.name = QCoreApplication::translate("toupie::Tournament", "Bot %1").arg(i + 1);
        bot.level = settings.bots.at(i);
        for (int k : randomOrder(static_cast<int>(pool.size()), static_cast<std::uint32_t>(rng())))
            if (bot.tops.size() < tops)
                bot.tops.append(pool.at(k));
        while (!pool.isEmpty() && bot.tops.size() < tops)
            bot.tops.append(pool.first()); // Moins de toupies que nécessaire.
        humans.append(bot);
    }
    return humans;
}

MatchSeries playBotMatch(const Participant &a, const Participant &b, MatchFormat format,
                         const Stadium &stadium, const MatchRules &rules, std::uint32_t seed)
{
    MatchSeries series;
    series.format = format;
    series.order[0] = randomOrder(static_cast<int>(a.tops.size()), seed);
    series.order[1] = randomOrder(static_cast<int>(b.tops.size()), seed + 1);
    std::mt19937 rng(seed);
    const double dt = 1.0 / 60.0;
    while (!series.isOver()) {
        Battle battle({a.tops.value(series.topFor(0)), b.tops.value(series.topFor(1))}, stadium,
                      rules.apply(BattleConfig{}), static_cast<std::uint32_t>(rng()));
        BotController bots[2] = {{0, a.level, static_cast<std::uint32_t>(rng())},
                                 {1, b.level, static_cast<std::uint32_t>(rng())}};
        battle.launch({bots[0].chooseLaunchAngle(battle), bots[1].chooseLaunchAngle(battle)},
                      {bots[0].chooseLaunchPower(battle), bots[1].chooseLaunchPower(battle)});
        while (!battle.isFinished()) {
            for (BotController &bot : bots)
                bot.update(battle, dt);
            battle.advance(dt);
        }
        series.recordRound(battle.winner(), battle.endReason());
    }
    return series;
}

} // namespace toupie
