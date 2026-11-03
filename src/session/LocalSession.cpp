#include "session/LocalSession.h"

#include <QRandomGenerator>

namespace toupie {

LocalSession::LocalSession(const QList<Top> &tops, const QStringList &names,
                           const Stadium &stadium, Difficulty difficulty,
                           const MatchRules &rules, QObject *parent)
    : MatchSession(parent)
    , m_tops(tops)
    , m_names(names)
    , m_stadium(stadium)
    , m_difficulty(difficulty)
    , m_rules(rules)
{
    restart();
}

LocalSession::~LocalSession() = default;

void LocalSession::restart()
{
    auto *rng = QRandomGenerator::global();
    m_battle = std::make_unique<Battle>(m_tops, m_stadium, m_rules.apply(BattleConfig{}),
                                        rng->generate());
    initFromBattle(*m_battle, m_names);
    m_state.rules = m_rules;
    m_localPlayer = 0;

    // Les bots choisissent leur angle à l'avance, sans voir celui du joueur.
    m_bots.clear();
    m_angles = QList<double>(m_battle->playerCount(), 0.0);
    m_powers = QList<double>(m_battle->playerCount(), 1.0);
    for (int i = 1; i < m_battle->playerCount(); ++i) {
        m_bots.emplace_back(i, m_difficulty, rng->generate());
        m_angles[i] = m_bots.back().chooseLaunchAngle(*m_battle);
        m_powers[i] = m_bots.back().chooseLaunchPower(*m_battle);
        m_state.aimed[i] = true;
    }

    m_state.phase = MatchPhase::Aiming;
    emit phaseChanged();
}

void LocalSession::submitAim(double angle, double power)
{
    if (m_state.phase != MatchPhase::Aiming)
        return;
    m_angles[0] = angle;
    m_powers[0] = power;
    m_state.aimed[0] = true;
    m_state.countdown = kCountdownSeconds;
    setPhase(MatchPhase::Countdown);
}

void LocalSession::requestDash()
{
    if (m_state.phase == MatchPhase::Fighting)
        m_battle->requestDash(0);
}

void LocalSession::requestSpecial()
{
    if (m_state.phase == MatchPhase::Fighting)
        m_battle->requestSpecial(0);
}

void LocalSession::setSteering(Vec2 direction)
{
    m_battle->setSteering(0, direction);
}

void LocalSession::update(double dt)
{
    switch (m_state.phase) {
    case MatchPhase::Aiming:
    case MatchPhase::Finished:
        break;
    case MatchPhase::Countdown:
        m_state.countdown -= dt;
        if (m_state.countdown <= 0.0) {
            m_state.countdown = 0.0;
            m_battle->launch(m_angles, m_powers);
            syncFromBattle(*m_battle);
            setPhase(MatchPhase::Fighting);
        }
        break;
    case MatchPhase::Fighting:
        for (BotController &bot : m_bots)
            bot.update(*m_battle, dt);
        m_battle->advance(dt);
        syncFromBattle(*m_battle);
        if (m_battle->isFinished())
            setPhase(MatchPhase::Finished);
        break;
    }
}

} // namespace toupie
