#include "net/TournamentSession.h"

#include "core/PartCatalog.h"
#include "net/ClientSession.h"
#include "net/HostSession.h"
#include "net/LobbyClient.h"
#include "net/LobbyServer.h"
#include "net/MatchCodec.h"

#include <QJsonArray>

#include <algorithm>
#include <numeric>

namespace toupie::net {

namespace {
/// Ordre valide (permutation de 0..count-1) ou ordre par défaut.
QList<int> sanitizeOrder(const QList<int> &order, int count)
{
    QList<int> sorted = order;
    std::sort(sorted.begin(), sorted.end());
    QList<int> expected(count);
    std::iota(expected.begin(), expected.end(), 0);
    return sorted == expected ? order : expected;
}

QJsonObject liveToJson(const TournamentController::LiveMatch &live)
{
    return {{"m", live.match},
            {"series", live.series.toJson()},
            {"r", QJsonArray{live.ready[0], live.ready[1]}}};
}
} // namespace

// ---------------------------------------------------------------------------
// TournamentController
// ---------------------------------------------------------------------------

TournamentController::TournamentController(int localPlayerId, QObject *parent)
    : QObject(parent)
    , m_localPlayerId(localPlayerId)
{
}

const TournamentController::LiveMatch *TournamentController::liveOf(int match) const
{
    for (const LiveMatch &live : m_live) {
        if (live.match == match)
            return &live;
    }
    return nullptr;
}

int TournamentController::localMatch() const
{
    const int match = m_tournament.matchOf(localParticipant());
    if (match < 0 || m_tournament.rounds().at(m_tournament.currentRound()).at(match).played)
        return -1;
    return match;
}

bool TournamentController::localReady() const
{
    const int match = localMatch();
    const LiveMatch *live = liveOf(match);
    if (!live)
        return false;
    const TournamentMatch &m = m_tournament.rounds().at(m_tournament.currentRound()).at(match);
    return live->ready[m.a == localParticipant() ? 0 : 1];
}

// ---------------------------------------------------------------------------
// TournamentDirector
// ---------------------------------------------------------------------------

TournamentDirector::TournamentDirector(LobbyServer *server, Tournament tournament,
                                       std::uint32_t seed, QObject *parent)
    : TournamentController(LobbyServer::kHostId, parent)
    , m_server(server)
    , m_rng(seed)
{
    m_tournament = std::move(tournament);
    if (m_server) {
        connect(m_server, &LobbyServer::gameMessage, this, &TournamentDirector::onGameMessage);
        connect(m_server, &LobbyServer::playerLeft, this, &TournamentDirector::onPlayerLeft);
    }
    // Les manches qui ne sont pas affichées ici sont simulées par ce minuteur.
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &TournamentDirector::tick);
    m_timer.start();
    m_clock.start();
    startRound();
}

TournamentDirector::~TournamentDirector() = default;

TournamentDirector::Match *TournamentDirector::find(int id)
{
    for (auto &m : m_matches) {
        if (m->id == id)
            return m.get();
    }
    return nullptr;
}

TournamentDirector::Match *TournamentDirector::findByParticipant(int participant)
{
    const QList<TournamentMatch> &round = m_tournament.rounds().at(m_tournament.currentRound());
    for (auto &m : m_matches) {
        const TournamentMatch &tm = round.at(m->index);
        if (participant >= 0 && (tm.a == participant || tm.b == participant))
            return m.get();
    }
    return nullptr;
}

void TournamentDirector::startRound()
{
    m_matches.clear();
    const TournamentSettings &settings = m_tournament.settings();
    const Stadium stadium = Stadium::find(settings.stadiumId).value_or(Stadium::classic());
    const QList<TournamentMatch> round = m_tournament.rounds().at(m_tournament.currentRound());
    const int tops = MatchSeries::topsFor(settings.matchFormat);
    for (int i = 0; i < round.size(); ++i) {
        const TournamentMatch &tm = round.at(i);
        if (tm.played)
            continue;
        const Participant &a = m_tournament.participants().at(tm.a);
        const Participant &b = m_tournament.participants().at(tm.b);
        if (a.isBot() && b.isBot()) {
            m_tournament.record(i, playBotMatch(a, b, settings.matchFormat, stadium,
                                                settings.rules, static_cast<std::uint32_t>(m_rng())));
            continue;
        }
        auto match = std::make_unique<Match>();
        match->id = m_nextId++;
        match->index = i;
        match->series.format = settings.matchFormat;
        for (int side = 0; side < 2; ++side) {
            if ((side == 0 ? a : b).isBot()) {
                match->series.order[side] = randomOrder(tops, static_cast<std::uint32_t>(m_rng()));
                match->ready[side] = true;
            }
        }
        m_matches.push_back(std::move(match));
    }
    publish();
    scheduleNextRound();
}

void TournamentDirector::scheduleNextRound()
{
    if (m_advancing || !m_tournament.roundComplete() || m_tournament.isFinished())
        return;
    m_advancing = true;
    QTimer::singleShot(1200, this, [this] {
        m_advancing = false;
        m_tournament.nextRound();
        startRound();
    });
}

void TournamentDirector::submitOrder(const QList<int> &order)
{
    setReady(localParticipant(), order);
}

void TournamentDirector::setReady(int participant, const QList<int> &order)
{
    Match *match = findByParticipant(participant);
    if (!match || match->session)
        return;
    const TournamentMatch &tm = m_tournament.rounds().at(m_tournament.currentRound()).at(match->index);
    const int side = tm.a == participant ? 0 : 1;
    if (match->ready[side])
        return;
    const int count = static_cast<int>(m_tournament.participants().at(participant).tops.size());
    match->series.order[side] = sanitizeOrder(order, count);
    match->ready[side] = true;
    publish();
    if (match->ready[0] && match->ready[1])
        playNextBattle(*match);
}

void TournamentDirector::playNextBattle(Match &match)
{
    match.owned.reset(); // Manche précédente.
    const TournamentSettings &settings = m_tournament.settings();
    const TournamentMatch &tm = m_tournament.rounds().at(m_tournament.currentRound()).at(match.index);

    MatchSetup setup;
    setup.stadium = Stadium::find(settings.stadiumId).value_or(Stadium::classic());
    setup.rules = settings.rules;
    setup.seed = static_cast<quint32>(m_rng());
    setup.match = match.index;
    setup.series = match.series;
    for (int side = 0; side < 2; ++side) {
        const Participant &p = m_tournament.participants().at(side == 0 ? tm.a : tm.b);
        setup.playerIds.append(p.playerId);
        setup.names.append(p.name);
        setup.tops.append(p.tops.value(match.series.topFor(side), p.tops.value(0)));
        setup.botLevels.append(p.isBot() ? static_cast<int>(p.level) : -1);
    }
    // Le message de départ précède le premier état envoyé par la session.
    if (m_server) {
        const QJsonObject start = encodeStart(setup);
        for (int id : setup.playerIds) {
            if (id != LobbyServer::kHostId && id >= 0)
                m_server->sendTo(id, start);
        }
    }
    std::unique_ptr<MatchSession> session = std::make_unique<HostSession>(m_server, setup);
    match.session = session.get();
    const int id = match.id;
    connect(session.get(), &MatchSession::finished, this, [this, id] { onBattleFinished(id); });

    if (setup.playerIds.contains(LobbyServer::kHostId)) {
        m_session = std::move(session);
        emit sessionReady();
        session = std::move(m_session); // Non récupérée : simulée ici.
    }
    match.owned = std::move(session);
}

void TournamentDirector::onBattleFinished(int id)
{
    Match *match = find(id);
    if (!match || !match->session)
        return;
    const MatchState &state = match->session->state();
    match->series.recordRound(state.winner, state.endReason);
    publish();
    QTimer::singleShot(kPauseMs, this, [this, id] {
        Match *m = find(id);
        if (!m)
            return;
        if (m->series.isOver())
            finishMatch(id);
        else
            playNextBattle(*m);
    });
}

void TournamentDirector::forfeitLocal()
{
    forfeit(localParticipant());
}

void TournamentDirector::forfeit(int participant)
{
    Match *match = findByParticipant(participant);
    if (!match)
        return;
    const TournamentMatch &tm = m_tournament.rounds().at(m_tournament.currentRound()).at(match->index);
    match->series.forfeit(tm.a == participant ? 0 : 1);
    finishMatch(match->id);
}

void TournamentDirector::finishMatch(int id)
{
    const auto it = std::find_if(m_matches.begin(), m_matches.end(),
                                 [id](const auto &m) { return m->id == id; });
    if (it == m_matches.end())
        return;
    const TournamentMatch &tm = m_tournament.rounds().at(m_tournament.currentRound()).at((*it)->index);
    const int local = localParticipant();
    const bool wasLocal = local >= 0 && (tm.a == local || tm.b == local);
    m_tournament.record((*it)->index, (*it)->series);
    m_matches.erase(it);
    publish();
    if (wasLocal)
        emit localMatchOver();
    scheduleNextRound();
}

void TournamentDirector::tick()
{
    const double dt = std::min(0.05, m_clock.restart() / 1000.0);
    // Une session peut se terminer (et la liste changer) pendant update().
    QList<QPointer<MatchSession>> sessions;
    for (auto &m : m_matches) {
        if (m->owned)
            sessions.append(m->owned.get());
    }
    for (const QPointer<MatchSession> &session : sessions) {
        if (session)
            session->update(dt);
    }
}

void TournamentDirector::publish()
{
    m_live.clear();
    for (const auto &m : m_matches)
        m_live.append({m->index, m->series, m->ready});
    emit changed();
    if (!m_server)
        return;
    QJsonArray live;
    for (const LiveMatch &l : m_live)
        live.append(liveToJson(l));
    m_server->broadcast(
        {{"type", Msg::Tournament}, {"t", m_tournament.toJson()}, {"live", live}});
}

void TournamentDirector::onGameMessage(int playerId, const QJsonObject &message)
{
    const QString type = message.value("type").toString();
    const int participant = m_tournament.participantOf(playerId);
    if (type == Msg::Order) {
        QList<int> order;
        for (const QJsonValue &v : message.value("order").toArray())
            order.append(v.toInt());
        setReady(participant, order);
    } else if (type == Msg::Forfeit) {
        forfeit(participant);
    }
}

void TournamentDirector::onPlayerLeft(int playerId)
{
    // Le joueur parti est remplacé par un bot pour la suite du tournoi (sa
    // manche en cours est perdue par abandon).
    const int participant = m_tournament.participantOf(playerId);
    if (participant < 0)
        return;
    Participant &p = m_tournament.participant(participant);
    p.playerId = -1;
    p.level = Difficulty::Normal;
    setReady(participant, randomOrder(static_cast<int>(p.tops.size()),
                                      static_cast<std::uint32_t>(m_rng())));
    publish();
}

// ---------------------------------------------------------------------------
// TournamentFollower
// ---------------------------------------------------------------------------

TournamentFollower::TournamentFollower(LobbyClient *client, QObject *parent)
    : TournamentController(client ? client->playerId() : -1, parent)
    , m_client(client)
{
    if (!m_client)
        return;
    connect(m_client, &LobbyClient::gameMessage, this, [this](const QJsonObject &message) {
        if (message.value("type").toString() == Msg::Tournament)
            apply(message);
    });
    connect(m_client, &LobbyClient::matchStarted, this, [this](const QJsonObject &start) {
        const auto setup = parseStart(start, PartCatalog::instance());
        if (!setup || setup->match < 0 || !m_client)
            return;
        m_playing = setup->match;
        m_session = std::make_unique<ClientSession>(m_client, *setup);
        emit sessionReady();
    });
}

void TournamentFollower::apply(const QJsonObject &message)
{
    auto tournament = Tournament::fromJson(message.value("t").toObject(), PartCatalog::instance());
    if (!tournament)
        return;
    m_tournament = std::move(*tournament);
    m_live.clear();
    for (const QJsonValue &v : message.value("live").toArray()) {
        const QJsonObject o = v.toObject();
        const QJsonArray ready = o.value("r").toArray();
        m_live.append({o.value("m").toInt(-1), MatchSeries::fromJson(o.value("series").toObject()),
                       {ready.at(0).toBool(), ready.at(1).toBool()}});
    }
    emit changed();
    if (m_playing >= 0 && !liveOf(m_playing)) {
        m_playing = -1;
        emit localMatchOver();
    }
}

void TournamentFollower::submitOrder(const QList<int> &order)
{
    QJsonArray list;
    for (int i : order)
        list.append(i);
    if (m_client)
        m_client->send({{"type", Msg::Order}, {"order", list}});
}

void TournamentFollower::forfeitLocal()
{
    if (m_client)
        m_client->send({{"type", Msg::Forfeit}});
}

} // namespace toupie::net
