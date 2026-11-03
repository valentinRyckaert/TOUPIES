#include "game/BotController.h"

#include "game/Battle.h"

#include <algorithm>
#include <cmath>

namespace toupie {

QString toString(Difficulty difficulty)
{
    switch (difficulty) {
    case Difficulty::Easy:
        return QStringLiteral("Facile");
    case Difficulty::Normal:
        return QStringLiteral("Normal");
    case Difficulty::Hard:
        return QStringLiteral("Difficile");
    }
    return {};
}

BotController::BotController(int index, Difficulty difficulty, std::uint32_t seed)
    : m_index(index)
    , m_difficulty(difficulty)
    , m_rng(seed)
{
}

double BotController::reactionTime() const
{
    switch (m_difficulty) {
    case Difficulty::Easy:
        return 0.5;
    case Difficulty::Normal:
        return 0.25;
    case Difficulty::Hard:
        break;
    }
    return 0.1;
}

double BotController::aimError() const
{
    switch (m_difficulty) {
    case Difficulty::Easy:
        return 0.5;
    case Difficulty::Normal:
        return 0.2;
    case Difficulty::Hard:
        break;
    }
    return 0.07;
}

double BotController::chooseLaunchAngle(const Battle &battle)
{
    const Spinner &self = battle.spinner(m_index);
    const Vec2 start = battle.startPosition(m_index);
    const Vec2 toCenter = (-start).normalized();
    // Direction naturelle de déplacement : opposée au sens de rotation.
    const Vec2 orbit = start.perpendicular().normalized() * -self.spinSign;

    double centerWeight = 0.6;
    double spread = 0.3;
    switch (classify(self.stats)) {
    case TopType::Attack:
        centerWeight = 1.0; // Droit sur l'adversaire.
        spread = 0.35;
        break;
    case TopType::Defense:
        centerWeight = 0.7; // Se placer au centre.
        spread = 0.15;
        break;
    case TopType::Endurance:
        centerWeight = 0.5; // Trajectoire en orbite, loin des chocs frontaux.
        spread = 0.15;
        break;
    case TopType::Balance:
        break;
    }

    const Vec2 direction = toCenter * centerWeight + orbit * (1.0 - centerWeight);
    std::uniform_real_distribution<double> variation(-spread, spread);
    std::normal_distribution<double> error(0.0, aimError());
    return direction.angle() + variation(m_rng) + error(m_rng);
}

void BotController::update(Battle &battle, double dt)
{
    if (battle.phase() != Battle::Phase::Running)
        return;

    m_decisionTimer -= dt;
    if (m_decisionTimer > 0.0)
        return;

    std::uniform_real_distribution<double> jitter(0.7, 1.3);
    m_decisionTimer = reactionTime() * jitter(m_rng);

    if (battle.canDash(m_index) && wantsToDash(battle))
        battle.requestDash(m_index);
}

bool BotController::wantsToDash(const Battle &battle)
{
    const BattleConfig &cfg = battle.config();
    const Spinner &self = battle.spinner(m_index);
    const int target = battle.nearestOpponent(m_index);
    if (target < 0)
        return false;
    const Spinner &other = battle.spinner(target);
    const StadiumGeometry &geometry = battle.geometry();

    const Vec2 delta = other.position - self.position;
    const double distance = delta.length();
    const Vec2 direction = delta.normalized();
    const double reach = cfg.dashDistance();
    double gap = distance - 2.0 * cfg.topRadius;

    // Anticipation : en difficile, on estime où sera l'adversaire.
    if (m_difficulty == Difficulty::Hard) {
        const Vec2 predicted = other.position + other.velocity * (gap / cfg.dashSpeed);
        gap = std::max(gap, (predicted - self.position).length() - 2.0 * cfg.topRadius);
    }

    const double rangeFactor = m_difficulty == Difficulty::Easy ? 1.3 : 0.85;
    if (gap > reach * rangeFactor)
        return false;

    // Ne pas se jeter soi-même hors du stadium (plus prudent sans rebord).
    const Vec2 landing = self.position + direction * (std::min(gap, reach) + 0.02);
    const double safeLevel = battle.stadium().hasRim ? 0.88 : 0.8;
    if (geometry.level(landing) > safeLevel)
        return false;

    if (m_difficulty == Difficulty::Easy) {
        std::bernoulli_distribution hesitate(0.35);
        if (hesitate(m_rng))
            return false;
    }

    const double opponentEdge = geometry.level(other.position);
    const double opponentIncoming = other.velocity.dot(-direction);
    const double myRatio = self.spinRatio();
    const double theirRatio = other.spinRatio();
    const double spinAfterDash = self.spin - cfg.dashCost;

    // Si l'adversaire va tenir plus longtemps à la seule endurance, attendre
    // ne sert à rien : on passe à l'offensive quel que soit le style.
    // En mêlée, on se compare à l'adversaire le plus endurant.
    const double myTimeLeft = self.spin / battle.passiveLossPerSecond(self);
    double theirTimeLeft = 0.0;
    for (int i = 0; i < battle.playerCount(); ++i) {
        const Spinner &s = battle.spinner(i);
        if (i != m_index && s.isActive())
            theirTimeLeft = std::max(theirTimeLeft, s.spin / battle.passiveLossPerSecond(s));
    }
    if (myTimeLeft < 0.9 * theirTimeLeft)
        return spinAfterDash > 10.0;

    switch (classify(self.stats)) {
    case TopType::Attack:
        return spinAfterDash > 10.0;
    case TopType::Defense:
        return spinAfterDash > 20.0 && (opponentEdge > 0.55 || opponentIncoming > 0.5);
    case TopType::Endurance:
        return spinAfterDash > 30.0 && opponentEdge > 0.65 && myRatio > theirRatio;
    case TopType::Balance:
        return spinAfterDash > 15.0 && (opponentEdge > 0.5 || myRatio >= theirRatio);
    }
    return false;
}

} // namespace toupie
