#include "net/ClientSession.h"

#include "net/LobbyClient.h"

#include <cmath>
#include <algorithm>

namespace toupie::net {

namespace {
/// Au-delà, on cesse d'extrapoler (paquets en retard).
constexpr double kMaxExtrapolation = 0.15;
constexpr double kPi = 3.14159265358979323846;
} // namespace

ClientSession::ClientSession(LobbyClient *client, const MatchSetup &setup, QObject *parent)
    : MatchSession(parent)
    , m_client(client)
{
    // Un combat local (jamais lancé) fournit stadium, constantes et positions de départ.
    const Battle reference(setup.tops, setup.stadium, setup.rules.apply(BattleConfig{}),
                           setup.seed);
    initFromBattle(reference, setup.names);
    m_state.rules = setup.rules;
    m_localPlayer = client ? std::max(0, static_cast<int>(setup.playerIds.indexOf(client->playerId())))
                           : 0;

    if (m_client) {
        connect(m_client, &LobbyClient::gameMessage, this, [this](const QJsonObject &message) {
            if (message.value("type").toString() == Msg::State)
                applyState(message);
        });
    }
}

void ClientSession::applyState(const QJsonObject &message)
{
    const MatchPhase before = m_state.phase;
    const bool localAimed = localHasAimed();
    if (!decodeState(message, m_state, &m_impacts))
        return;
    // Ne pas « dé-valider » une visée envoyée mais pas encore prise en compte.
    if (localAimed && m_state.phase == MatchPhase::Aiming)
        m_state.aimed[m_localPlayer] = true;
    m_sinceState = 0.0;

    if (m_state.phase != before) {
        const MatchPhase now = m_state.phase;
        m_state.phase = before;
        setPhase(now);
    }
}

void ClientSession::update(double dt)
{
    m_sinceState += dt;
    if (m_state.phase == MatchPhase::Countdown)
        m_state.countdown = std::max(0.0, m_state.countdown - dt);

    if (m_state.phase != MatchPhase::Fighting || m_sinceState > kMaxExtrapolation)
        return;

    // Extrapolation entre deux états : déplacement à vitesse constante.
    m_state.elapsed += dt;
    for (Projectile &p : m_state.projectiles) {
        if (p.ring)
            p.radius += p.speed * dt;
        else
            p.position += p.direction * (p.speed * dt);
    }
    for (Spinner &s : m_state.spinners) {
        if (!s.isActive())
            continue;
        s.position += s.velocity * dt;
        s.angle = std::fmod(s.angle + s.spinSign * (6.0 + 30.0 * s.spinRatio()) * dt, 2.0 * kPi);
        s.dashCooldown = std::max(0.0, s.dashCooldown - dt);
        s.boostTimer = std::max(0.0, s.boostTimer - dt);
    }
}

void ClientSession::submitAim(double angle, double power)
{
    if (m_state.phase != MatchPhase::Aiming || localHasAimed())
        return;
    m_state.aimed[m_localPlayer] = true;
    if (m_client)
        m_client->send({{"type", Msg::Aim}, {"angle", angle}, {"power", power}});
}

void ClientSession::requestSpecial()
{
    if (m_state.phase == MatchPhase::Fighting && m_client)
        m_client->send({{"type", Msg::Special}});
}

void ClientSession::setSteering(Vec2 direction)
{
    // N'envoie que les changements (touches enfoncées / relâchées).
    if ((direction - m_steer).lengthSquared() < 1e-9 || !m_client)
        return;
    m_steer = direction;
    m_client->send({{"type", Msg::Steer}, {"x", direction.x}, {"y", direction.y}});
}

void ClientSession::requestDash()
{
    if (m_state.phase == MatchPhase::Fighting && m_client)
        m_client->send({{"type", Msg::Dash}});
}

} // namespace toupie::net
