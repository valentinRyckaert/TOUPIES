#include "net/HostSession.h"

#include "net/LobbyServer.h"

#include <QRandomGenerator>

#include <algorithm>

namespace toupie::net {

HostSession::HostSession(LobbyServer *server, const MatchSetup &setup, QObject *parent)
    : MatchSession(parent)
    , m_server(server)
    , m_setup(setup)
    , m_battle(std::make_unique<Battle>(setup.tops, setup.stadium,
                                        setup.rules.apply(BattleConfig{}), setup.seed))
{
    initFromBattle(*m_battle, setup.names);
    m_state.rules = setup.rules;
    m_state.series = setup.series;
    m_localPlayer = std::max(0, slotOf(LobbyServer::kHostId));
    m_angles.clear();
    for (int i = 0; i < m_battle->playerCount(); ++i) {
        m_angles.append((-m_battle->startPosition(i)).angle()); // Défaut : vers le centre.
        m_powers.append(1.0);
        if (const int level = setup.botLevels.value(i, -1); level >= 0) {
            m_bots.emplace_back(i, static_cast<Difficulty>(level),
                                QRandomGenerator::global()->generate());
            m_angles[i] = m_bots.back().chooseLaunchAngle(*m_battle);
            m_powers[i] = m_bots.back().chooseLaunchPower(*m_battle);
            m_state.aimed[i] = true;
        }
    }

    if (m_server) {
        connect(m_server, &LobbyServer::gameMessage, this, &HostSession::onGameMessage);
        connect(m_server, &LobbyServer::playerLeft, this, &HostSession::onPlayerLeft);
    }
    sendState();
}

HostSession::~HostSession() = default;

int HostSession::slotOf(int playerId) const
{
    return static_cast<int>(m_setup.playerIds.indexOf(playerId));
}

void HostSession::setAim(int slot, double angle, double power)
{
    if (m_state.phase != MatchPhase::Aiming || slot < 0 || slot >= m_angles.size()
        || m_state.aimed.value(slot))
        return;
    m_angles[slot] = angle;
    m_powers[slot] = power;
    m_state.aimed[slot] = true;
    m_sendTimer = 1.0; // Diffusion immédiate.
}

void HostSession::submitAim(double angle, double power)
{
    setAim(m_localPlayer, angle, power);
}

void HostSession::requestSpecial()
{
    if (m_state.phase == MatchPhase::Fighting)
        m_battle->requestSpecial(m_localPlayer);
}

void HostSession::setSteering(Vec2 direction)
{
    m_battle->setSteering(m_localPlayer, direction);
}

void HostSession::requestDash()
{
    if (m_state.phase == MatchPhase::Fighting)
        m_battle->requestDash(m_localPlayer);
}

void HostSession::onGameMessage(int playerId, const QJsonObject &message)
{
    const int slot = slotOf(playerId);
    if (slot < 0)
        return;
    const QString type = message.value("type").toString();
    if (type == Msg::Aim) {
        setAim(slot, message.value("angle").toDouble(), message.value("power").toDouble(1.0));
    } else if (type == Msg::Steer) {
        m_battle->setSteering(slot, {message.value("x").toDouble(), message.value("y").toDouble()});
    } else if (m_state.phase == MatchPhase::Fighting) {
        if (type == Msg::Dash)
            m_battle->requestDash(slot);
        else if (type == Msg::Special)
            m_battle->requestSpecial(slot);
        else if (type == Msg::Duel)
            m_battle->duelPress(slot, message.value("c").toDouble());
    }
}

void HostSession::onPlayerLeft(int playerId)
{
    const int slot = slotOf(playerId);
    if (slot < 0 || m_state.phase == MatchPhase::Finished)
        return;
    m_battle->forfeit(slot);
    m_state.aimed[slot] = true;
    syncFromBattle(*m_battle);
    if (m_battle->isFinished())
        setPhase(MatchPhase::Finished); // Tous les adversaires sont partis.
    m_sendTimer = 1.0;
}

void HostSession::update(double dt)
{
    const MatchPhase before = m_state.phase;
    switch (m_state.phase) {
    case MatchPhase::Aiming: {
        m_aimTimer += dt;
        const bool everyone = !m_state.aimed.contains(false);
        // Sans serveur (combat local), le joueur prend tout son temps.
        if (everyone || (m_server && m_aimTimer >= kAimTimeout)) {
            for (bool &aimed : m_state.aimed)
                aimed = true; // Les retardataires partent vers le centre.
            m_state.countdown = kCountdownSeconds;
            setPhase(MatchPhase::Countdown);
        }
        break;
    }
    case MatchPhase::Countdown:
        m_state.countdown -= dt;
        if (m_state.countdown <= 0.0) {
            m_state.countdown = 0.0;
            m_battle->launch(m_angles, m_powers);
            m_pendingImpacts.append(syncFromBattle(*m_battle));
            setPhase(m_battle->isFinished() ? MatchPhase::Finished : MatchPhase::Fighting);
        }
        break;
    case MatchPhase::Fighting:
        for (BotController &bot : m_bots)
            bot.update(*m_battle, dt);
        m_battle->advance(dt);
        m_pendingImpacts.append(syncFromBattle(*m_battle));
        if (m_battle->isFinished())
            setPhase(MatchPhase::Finished);
        break;
    case MatchPhase::Finished:
        break;
    }

    // Après la fin, un envoi par seconde suffit à confirmer le résultat.
    m_sendTimer += dt;
    const double interval = m_state.phase == MatchPhase::Finished ? 1.0 : 1.0 / kStateRate;
    if (m_state.phase != before || m_sendTimer >= interval)
        sendState();
}

void HostSession::sendState()
{
    m_sendTimer = 0.0;
    if (m_server) {
        // Seuls les participants reçoivent l'état (plusieurs matchs de
        // tournoi peuvent se jouer en même temps).
        const QJsonObject state = encodeState(m_state, m_pendingImpacts);
        for (int id : m_setup.playerIds) {
            if (id != LobbyServer::kHostId && id >= 0)
                m_server->sendTo(id, state);
        }
    }
    m_pendingImpacts.clear();
}

} // namespace toupie::net
