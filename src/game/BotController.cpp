#include "game/BotController.h"

#include "game/Battle.h"
#include "game/SpecialMove.h"

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

double BotController::chooseLaunchPower(const Battle &battle)
{
    const Spinner &self = battle.spinner(m_index);
    double power = 1.0;
    switch (classify(self.stats)) {
    case TopType::Attack:
        power = 1.3; // Arriver vite au contact.
        break;
    case TopType::Defense:
        power = 0.85; // Se poser au centre.
        break;
    case TopType::Endurance:
        power = 0.75; // Économiser sa rotation.
        break;
    case TopType::Balance:
        break;
    }
    std::normal_distribution<double> error(0.0, aimError() * 0.5);
    const BattleConfig &cfg = battle.config();
    return std::clamp(power + error(m_rng), cfg.launchPowerMin, cfg.launchPowerMax);
}

double BotController::dodgeSkill() const
{
    switch (m_difficulty) {
    case Difficulty::Easy:
        return 0.2;
    case Difficulty::Normal:
        return 0.6;
    case Difficulty::Hard:
        break;
    }
    return 0.75;
}

void BotController::update(Battle &battle, double dt)
{
    if (battle.phase() != Battle::Phase::Running)
        return;

    // Pilotage : réévalué fréquemment (esquive comprise).
    m_steerTimer -= dt;
    if (m_steerTimer <= 0.0) {
        // Même cadence pour toutes les difficultés : un pilotage trop réactif
        // rend la fuite imparable.
        m_steerTimer = 0.12;
        battle.setSteering(m_index, chooseSteering(battle));
    }

    m_decisionTimer -= dt;
    if (m_decisionTimer > 0.0)
        return;

    std::uniform_real_distribution<double> jitter(0.7, 1.3);
    m_decisionTimer = reactionTime() * jitter(m_rng);

    if (battle.canSpecial(m_index) && wantsSpecial(battle)) {
        battle.requestSpecial(m_index);
        return;
    }
    if (!battle.canDash(m_index))
        return;
    const int target = chooseDashTarget(battle);
    if (target >= 0)
        battle.requestDash(m_index, target);
}

bool BotController::wantsSpecial(const Battle &battle)
{
    const Spinner &self = battle.spinner(m_index);
    const auto move = SpecialMove::find(self.top.special());
    const int target = battle.nearestOpponent(m_index);
    if (!move || target < 0)
        return false;
    const Spinner &other = battle.spinner(target);
    const StadiumGeometry &geometry = battle.geometry();
    const double distance = (other.position - self.position).length();
    const double reach = move->range * (m_difficulty == Difficulty::Easy ? 1.2 : 0.8);
    if (distance > reach)
        return false;

    switch (move->effect) {
    case SpecialMove::Effect::Teleport:
        // Inutile si la cible est déjà au bord.
        return geometry.level(other.position) < 0.5;
    case SpecialMove::Effect::PullToCenter:
        // Pour forcer le contact quand l'adversaire fuit loin.
        return distance > 0.45;
    case SpecialMove::Effect::Drain:
        return self.spinRatio() < 0.9;
    default:
        break;
    }
    if (move->shape == SpecialMove::Shape::Ring && move->knockback > 0.5)
        return geometry.level(other.position) > 0.35; // Onde : pousser vers le bord.
    return true;
}

Vec2 BotController::dodgeDirection(const Battle &battle)
{
    const BattleConfig &cfg = battle.config();
    const Spinner &self = battle.spinner(m_index);
    for (const Projectile &p : battle.projectiles()) {
        if (p.owner == m_index || p.hit.contains(m_index))
            continue;
        const int key = p.id;
        if (!m_seenThreats.contains(key)) {
            // Selon son niveau, le bot remarque ou non chaque vague.
            m_seenThreats.append(key);
            std::bernoulli_distribution notice(dodgeSkill());
            if (!notice(m_rng))
                m_ignoredThreats.append(key);
        }
        if (m_ignoredThreats.contains(key))
            continue;

        if (p.ring) {
            const Vec2 away = self.position - p.origin;
            const double d = away.length();
            // Fuir seulement si on peut sortir de la portée à temps.
            if (d > p.radius && d > p.range - 0.25 && d < p.range + cfg.topRadius)
                return away.normalized();
            continue;
        }
        const Vec2 toSelf = self.position - p.position;
        const double along = toSelf.dot(p.direction);
        if (along < 0.0 || along / p.speed > 1.2)
            continue; // Déjà passée, ou encore loin.
        const Vec2 lateral = toSelf - p.direction * along;
        if (lateral.length() > cfg.topRadius + p.width + 0.08)
            continue; // Ne passera pas sur nous.
        // S'écarter perpendiculairement à la trajectoire, du côté où l'on est déjà.
        Vec2 side = lateral.lengthSquared() > 1e-8 ? lateral.normalized() : p.direction.perpendicular();
        // Mais pas vers le bord.
        if (battle.geometry().level(self.position + side * 0.2) > 0.85)
            side = -side;
        return side;
    }
    if (battle.projectiles().empty()) {
        m_seenThreats.clear();
        m_ignoredThreats.clear();
    }
    return {};
}

Vec2 BotController::chooseSteering(const Battle &battle)
{
    const Spinner &self = battle.spinner(m_index);
    const StadiumGeometry &geometry = battle.geometry();
    const double level = geometry.level(self.position);
    const Vec2 toCenter = (-self.position).normalized();

    // Sécurité d'abord : ne pas sortir du stadium.
    if (level > (battle.stadium().hasRim ? 0.8 : 0.7))
        return toCenter;

    const Vec2 dodge = dodgeDirection(battle);
    if (dodge.lengthSquared() > 0.0)
        return dodge;

    const int target = battle.nearestOpponent(m_index);
    if (target < 0)
        return {};
    const Spinner &other = battle.spinner(target);
    const Vec2 toTarget = other.position - self.position;
    const double distance = toTarget.length();

    switch (classify(self.stats)) {
    case TopType::Attack:
        return toTarget.normalized(); // Chercher le contact.
    case TopType::Defense:
        return level > 0.25 ? toCenter * 0.7 : Vec2{}; // Tenir le centre.
    case TopType::Endurance:
        if (distance < 0.4)
            return (-toTarget).normalized() * 0.8; // Éviter les chocs.
        return level > 0.5 ? toCenter * 0.5 : Vec2{};
    case TopType::Balance:
        if (self.spinRatio() > other.spinRatio())
            return toTarget.normalized() * 0.6;
        return level > 0.45 ? toCenter * 0.6 : Vec2{};
    }
    return {};
}

int BotController::chooseDashTarget(const Battle &battle)
{
    if (m_difficulty == Difficulty::Easy) {
        std::bernoulli_distribution hesitate(0.35);
        if (hesitate(m_rng))
            return -1;
    }

    int best = -1;
    double bestScore = -1e18;
    for (int i = 0; i < battle.playerCount(); ++i) {
        if (i == m_index || !battle.spinner(i).isActive())
            continue;
        double score = 0.0;
        if (wantsToDashAt(battle, i, &score) && score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

bool BotController::wantsToDashAt(const Battle &battle, int target, double *score)
{
    const BattleConfig &cfg = battle.config();
    const Spinner &self = battle.spinner(m_index);
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
        gap = std::min(gap, (predicted - self.position).length() - 2.0 * cfg.topRadius);
    }

    const double rangeFactor = m_difficulty == Difficulty::Easy ? 1.3 : 0.85;
    if (gap > reach * rangeFactor)
        return false;

    // Ne pas se jeter soi-même hors du stadium (plus prudent sans rebord).
    const Vec2 landing = self.position + direction * (std::min(gap, reach) + 0.02);
    const double safeLevel = battle.stadium().hasRim ? 0.88 : 0.8;
    if (geometry.level(landing) > safeLevel)
        return false;

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
    // Choix de la cible en mêlée : l'attaque vise les proies fragiles (peu de
    // défense, au bord, à court d'endurance), les autres styles la plus proche.
    const TopType style = classify(self.stats);
    if (style == TopType::Attack && m_difficulty != Difficulty::Easy) {
        *score = opponentEdge + (1.0 - theirRatio) * 0.5 - other.stats.defense / 15.0 - gap;
    } else {
        *score = -distance;
    }

    if (myTimeLeft < 0.9 * theirTimeLeft)
        return spinAfterDash > 10.0;

    switch (style) {
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
