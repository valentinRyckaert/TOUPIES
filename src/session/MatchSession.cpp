#include "session/MatchSession.h"

namespace toupie {

MatchSession::MatchSession(QObject *parent)
    : QObject(parent)
{
}

QList<ImpactEvent> MatchSession::takeImpacts()
{
    QList<ImpactEvent> impacts;
    impacts.swap(m_impacts);
    return impacts;
}

void MatchSession::setPhase(MatchPhase phase)
{
    if (m_state.phase == phase)
        return;
    m_state.phase = phase;
    emit phaseChanged();
    if (phase == MatchPhase::Finished)
        emit finished(m_state.winner);
}

void MatchSession::initFromBattle(const Battle &battle, const QStringList &names)
{
    m_state = MatchState{};
    m_state.stadium = battle.stadium();
    m_state.config = battle.config();
    m_state.geometry = battle.geometry();
    m_state.spinners = battle.spinners();
    m_state.names = names;
    for (int i = 0; i < battle.playerCount(); ++i) {
        m_state.startPositions.append(battle.startPosition(i));
        m_state.aimed.append(false);
        if (m_state.names.size() <= i)
            m_state.names.append(QStringLiteral("Joueur %1").arg(i + 1));
    }
    m_impacts.clear();
}

QList<ImpactEvent> MatchSession::syncFromBattle(Battle &battle)
{
    m_state.spinners = battle.spinners();
    m_state.elapsed = battle.elapsed();
    m_state.winner = battle.winner();
    m_state.endReason = battle.endReason();
    m_state.ranking = battle.ranking();
    m_state.projectiles = battle.projectiles();
    m_state.duel = battle.duel();
    const QList<ImpactEvent> impacts = battle.takeImpacts();
    m_impacts.append(impacts);
    return impacts;
}

} // namespace toupie
