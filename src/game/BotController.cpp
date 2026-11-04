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

BotController::Skill BotController::skillFor(Difficulty difficulty)
{
    switch (difficulty) {
    case Difficulty::Easy:
        return {0.45, 0.5, 0.2, 1.3, 0.15, false, false, false, 0.075};
    case Difficulty::Normal:
        return {0.25, 0.2, 0.6, 0.9, 0.0, false, false, false, 0.045};
    case Difficulty::Hard:
        break;
    }
    return {0.12, 0.07, 0.8, 0.9, 0.0, true, true, true, 0.025};
}

BotController::BotController(int index, Difficulty difficulty, std::uint32_t seed)
    : m_index(index)
    , m_difficulty(difficulty)
    , m_skill(skillFor(difficulty))
    , m_rng(seed)
{
}

// ---------------------------------------------------------------------------
// Lancement
// ---------------------------------------------------------------------------

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
    if (m_skill.tactics && battle.playerCount() > 2) {
        // Mêlée : ne pas foncer au centre où tout le monde se percute.
        centerWeight = std::min(centerWeight, 0.55);
    }

    const Vec2 direction = toCenter * centerWeight + orbit * (1.0 - centerWeight);
    std::uniform_real_distribution<double> variation(-spread, spread);
    std::normal_distribution<double> error(0.0, m_skill.aimError);
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
    if (m_skill.tactics) {
        // Sans rebord, un lancer trop puissant peut finir dehors.
        if (!battle.stadium().hasRim)
            power = std::min(power, 1.0);
        // En mêlée, les chocs au centre éjectent les toupies trop rapides.
        if (battle.playerCount() > 2)
            power = std::min(power, 0.9);
    }
    std::normal_distribution<double> error(0.0, m_skill.aimError * 0.5);
    const BattleConfig &cfg = battle.config();
    return std::clamp(power + error(m_rng), cfg.launchPowerMin, cfg.launchPowerMax);
}

// ---------------------------------------------------------------------------
// Analyse
// ---------------------------------------------------------------------------

double BotController::enduranceRace(const Battle &battle) const
{
    const Spinner &self = battle.spinner(m_index);
    const double mine = self.spin / std::max(1e-6, battle.passiveLossPerSecond(self));
    double theirs = 0.0;
    for (int i = 0; i < battle.playerCount(); ++i) {
        const Spinner &s = battle.spinner(i);
        if (i != m_index && s.isActive())
            theirs = std::max(theirs, s.spin / std::max(1e-6, battle.passiveLossPerSecond(s)));
    }
    return theirs > 0.0 ? mine / theirs : 10.0;
}

int BotController::chooseTarget(const Battle &battle) const
{
    if (!m_skill.tactics || battle.playerCount() <= 2)
        return battle.nearestOpponent(m_index);

    // Mêlée : préférer les proies fragiles et proches du bord, sans aller trop loin.
    const Spinner &self = battle.spinner(m_index);
    const StadiumGeometry &geometry = battle.geometry();
    int best = -1;
    double bestScore = -1e18;
    for (int i = 0; i < battle.playerCount(); ++i) {
        const Spinner &s = battle.spinner(i);
        if (i == m_index || !s.isActive())
            continue;
        const double score = geometry.level(s.position) + (1.0 - s.spinRatio()) * 0.6
                           - s.stats.defense / 20.0 - (s.position - self.position).length() * 1.2;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

BotController::Plan BotController::choosePlan(const Battle &battle, int target) const
{
    const Spinner &self = battle.spinner(m_index);
    const TopType style = classify(self.stats);
    const double race = enduranceRace(battle);

    if (!m_skill.tactics) {
        // Plans simples, fixés par le type (avec repli offensif si on perd la course).
        if (race < 0.9)
            return Plan::Hunt;
        switch (style) {
        case TopType::Attack:
            return Plan::Hunt;
        case TopType::Endurance:
            return Plan::Evade;
        default:
            return Plan::Hold;
        }
    }

    // Tactique : la course à l'usure décide, le type nuance.
    const Spinner *other = target >= 0 ? &battle.spinner(target) : nullptr;
    const bool canFinish = other && other->spinRatio() < 0.15 && self.spinRatio() > 0.25;
    if (canFinish)
        return Plan::Hunt;
    switch (style) {
    case TopType::Attack:
        // Une attaque qui gagnerait à l'usure (rare) n'a rien à prouver.
        return race > 1.4 ? Plan::Hold : Plan::Hunt;
    case TopType::Defense:
        return race < 0.9 ? Plan::Hunt : Plan::Hold;
    case TopType::Endurance:
        return race < 0.85 ? Plan::Hunt : Plan::Evade;
    case TopType::Balance:
        return race < 1.0 ? Plan::Hunt : (race > 1.25 ? Plan::Evade : Plan::Hold);
    }
    return Plan::Hold;
}

// ---------------------------------------------------------------------------
// Boucle de décision
// ---------------------------------------------------------------------------

void BotController::update(Battle &battle, double dt)
{
    if (battle.phase() != Battle::Phase::Running || !battle.spinner(m_index).isActive())
        return;

    // Duel : appuyer quand le curseur passe au centre de la zone verte, avec
    // une erreur de timing qui dépend du niveau.
    const Duel &duel = battle.duel();
    if (const int side = duel.sideOf(m_index); side >= 0) {
        const DuelSide &bar = duel.sides[side];
        const double next = bar.cursor + bar.direction * bar.speed * dt;
        if (!bar.done && (bar.cursor < 0.0) != (next < 0.0)) {
            std::normal_distribution<double> timing(0.0, m_skill.duelTiming);
            battle.duelPress(m_index, timing(m_rng) * bar.speed);
        }
        return;
    }

    const int target = chooseTarget(battle);

    // Pilotage : même cadence pour toutes les difficultés (un pilotage trop
    // réactif rend la fuite imparable).
    m_steerTimer -= dt;
    if (m_steerTimer <= 0.0) {
        m_steerTimer = 0.12;
        m_plan = choosePlan(battle, target);
        battle.setSteering(m_index, chooseSteering(battle, target));
    }

    // Contre-dash (réflexe, hors cadence de décision) : répondre à un dash
    // adverse par un dash provoque un duel, où un bon timing fait la différence.
    if (m_skill.tactics && battle.canDash(m_index)) {
        const Spinner &self = battle.spinner(m_index);
        for (int j = 0; j < battle.playerCount(); ++j) {
            const Spinner &other = battle.spinner(j);
            const Vec2 toMe = self.position - other.position;
            if (j != m_index && other.isActive() && other.isDashing()
                && other.dashDirection.dot(toMe.normalized()) > 0.9
                && toMe.length() < battle.config().dashDistance() * 1.5) {
                battle.requestDash(m_index, j);
                return;
            }
        }
    }

    m_decisionTimer -= dt;
    if (m_decisionTimer > 0.0)
        return;
    std::uniform_real_distribution<double> jitter(0.7, 1.3);
    m_decisionTimer = m_skill.reaction * jitter(m_rng);

    if (m_specialsAllowed && battle.canSpecial(m_index)) {
        const double value = specialValue(battle);
        std::bernoulli_distribution hesitate(m_skill.hesitate);
        if (value > 0.0 && !hesitate(m_rng)) {
            battle.requestSpecial(m_index);
            return;
        }
    }
    if (!battle.canDash(m_index))
        return;
    const int dashTarget = chooseDashTarget(battle);
    if (dashTarget >= 0)
        battle.requestDash(m_index, dashTarget);
}

// ---------------------------------------------------------------------------
// Coup spécial
// ---------------------------------------------------------------------------

namespace {
/// Une poussée de vitesse `push` dans la direction `direction` éjecte-t-elle
/// probablement la toupie située en `position` ? Avec un rebord, il faut que
/// la vitesse vers l'extérieur dépasse le seuil de franchissement.
bool likelyEjection(const Battle &battle, Vec2 position, Vec2 direction, double push)
{
    const StadiumGeometry &geometry = battle.geometry();
    const double level = geometry.level(position);
    if (!battle.stadium().hasRim)
        return geometry.level(position + direction * (push * 0.45)) > 0.8;
    const Vec2 outward = geometry.outwardNormal(position);
    return level > 0.65 && push * direction.dot(outward) > battle.config().rimEscapeSpeed * 1.15;
}
} // namespace

double BotController::hitProbability(const Battle &battle, const SpecialMove &move,
                                     int target) const
{
    const BattleConfig &cfg = battle.config();
    const Spinner &self = battle.spinner(m_index);
    const Spinner &other = battle.spinner(target);
    const Vec2 delta = other.position - self.position;
    const double distance = delta.length();

    if (move.shape == SpecialMove::Shape::Ring)
        return distance + cfg.topRadius * 0.5 <= move.range ? 0.95 : 0.0;

    if (distance > move.range)
        return 0.0;
    if (other.isSlowed())
        return 0.95; // Ne peut pas piloter pour l'éviter.
    // Plus la vague met de temps et plus la cible bouge latéralement, moins on touche.
    const double travel = distance / move.speed;
    const Vec2 dir = delta.normalized();
    const double lateralSpeed = std::abs(other.velocity.dot(dir.perpendicular()));
    const double drift = lateralSpeed * travel + cfg.steerAcceleration * travel * travel * 0.5;
    const double tolerance = cfg.topRadius + move.width;
    return std::clamp(1.1 - drift / tolerance * 0.6, 0.15, 0.95);
}

double BotController::specialValue(const Battle &battle) const
{
    const BattleConfig &cfg = battle.config();
    const Spinner &self = battle.spinner(m_index);
    const auto move = SpecialMove::find(self.top.special());
    const int nearest = battle.nearestOpponent(m_index);
    if (!move || nearest < 0)
        return -1e9;

    const StadiumGeometry &geometry = battle.geometry();
    const double cost = cfg.specialCost * self.spin;

    if (m_difficulty == Difficulty::Easy) {
        // Débutant impulsif : lance son coup dès que l'adversaire est à portée
        // et qu'il a commencé à s'user, sans calculer.
        const double distance = (battle.spinner(nearest).position - self.position).length();
        return distance < move->range * 0.9 && self.spinRatio() < 0.8 ? 1.0 : -1.0;
    }

    // Cibles concernées : la plus proche (vague) ou toutes celles à portée (anneau).
    QList<int> targets;
    if (move->shape == SpecialMove::Shape::Ring) {
        for (int i = 0; i < battle.playerCount(); ++i) {
            if (i != m_index && battle.spinner(i).isActive())
                targets.append(i);
        }
    } else {
        targets.append(nearest);
    }

    double value = -cost;
    for (int i : targets) {
        const Spinner &t = battle.spinner(i);
        const double p = hitProbability(battle, *move, i);
        if (p <= 0.0)
            continue;
        const double mitigation = std::max(cfg.minReduction,
                                           1.0 - cfg.damagePerDefense * t.stats.defense);
        const double damage = std::min(t.spin, move->damage * cfg.specialDamageScale * mitigation);
        double gain = damage;
        if (move->effect == SpecialMove::Effect::Drain)
            gain += damage * move->effectStrength;
        if (move->effect == SpecialMove::Effect::Slow) // Usure due au gel.
            gain += battle.passiveLossPerSecond(t) * cfg.slowWear * move->effectStrength
                  * move->effectDuration;
        if (damage >= t.spin)
            gain += 40.0; // Coup fatal : la partie (ou un adversaire) est gagnée.

        // Éjection probable : recul vers le bord sur une cible déjà écartée.
        const double resistance = std::max(cfg.minReduction,
                                           1.0 - cfg.knockbackPerDefense * t.stats.defense);
        const double push = move->knockback * cfg.specialKnockbackScale * resistance;
        const double level = geometry.level(t.position);
        const Vec2 pushDirection = (t.position - self.position).normalized();
        if (move->effect != SpecialMove::Effect::Teleport && push > 0.3
            && likelyEjection(battle, t.position, pushDirection, push))
            gain += t.spin * 0.7;

        if (m_skill.tactics) {
            const bool dashReady = self.dashCooldown <= 0.0 && !self.isDisarmed();
            const double distance = (t.position - self.position).length();
            switch (move->effect) {
            case SpecialMove::Effect::Slow:
                // Combo : gel puis dash sur une cible qui ne peut plus fuir.
                if (dashReady && m_plan == Plan::Hunt)
                    gain += 12.0 + t.spin * 0.15;
                break;
            case SpecialMove::Effect::Teleport:
                // Combo : téléportée au bord, la cible est à portée d'éjection.
                if (level < 0.5 && classify(self.stats) != TopType::Endurance)
                    gain += t.spin * 0.3;
                break;
            case SpecialMove::Effect::PullToCenter:
                // Rattraper une toupie qui fuit, ou se protéger d'une poussée au bord.
                if (m_plan == Plan::Hunt && distance > 0.45)
                    gain += 15.0;
                break;
            case SpecialMove::Effect::Disarm:
                if (t.stats.attack >= 9 && distance < 0.45)
                    gain += 12.0;
                break;
            default:
                break;
            }
        }
        value += p * gain;
    }
    // Difficile : ne pas parier son endurance sur un coup incertain.
    if (m_skill.tactics) {
        const int main = targets.isEmpty() ? -1 : targets.first();
        const double p = main >= 0 ? hitProbability(battle, *move, main) : 0.0;
        if (move->shape == SpecialMove::Shape::Wave && p < 0.6)
            return -1.0;
        // Marge de sécurité : le coût (fraction de l'endurance) pèse moins en fin
        // de partie, la patience est donc payante.
        value -= cost * 0.6;
    }
    return value;
}

// ---------------------------------------------------------------------------
// Pilotage
// ---------------------------------------------------------------------------

Vec2 BotController::dodgeDirection(const Battle &battle)
{
    const BattleConfig &cfg = battle.config();
    const Spinner &self = battle.spinner(m_index);
    for (const Projectile &p : battle.projectiles()) {
        if (p.owner == m_index || p.hit.contains(m_index))
            continue;
        if (!m_seenThreats.contains(p.id)) {
            // Selon son niveau, le bot remarque ou non chaque vague.
            m_seenThreats.append(p.id);
            std::bernoulli_distribution notice(m_skill.dodgeWaves);
            if (!notice(m_rng))
                m_ignoredThreats.append(p.id);
        }
        if (m_ignoredThreats.contains(p.id))
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
        const double horizon = m_skill.anticipate ? 2.0 : 1.2;
        if (along < 0.0 || along / p.speed > horizon)
            continue; // Déjà passée, ou encore loin.
        // Où serons-nous quand la vague arrivera ? (on continue sur notre élan)
        const double eta = along / p.speed;
        const Vec2 future = m_skill.anticipate ? self.position + self.velocity * eta : self.position;
        const Vec2 lateralNow = toSelf - p.direction * along;
        const Vec2 toFuture = future - p.position;
        const Vec2 lateral = toFuture - p.direction * toFuture.dot(p.direction);
        const double tolerance = cfg.topRadius + p.width + 0.08;
        if (lateral.length() > tolerance && lateralNow.length() > tolerance)
            continue; // Ne passera pas sur nous.
        // S'écarter perpendiculairement à la trajectoire, du côté où l'élan nous porte déjà.
        const Vec2 perp = p.direction.perpendicular();
        Vec2 side;
        if (m_skill.anticipate && std::abs(self.velocity.dot(perp)) > 0.05)
            side = self.velocity.dot(perp) > 0.0 ? perp : -perp;
        else
            side = lateralNow.lengthSquared() > 1e-8 ? lateralNow.normalized() : perp;
        if (battle.geometry().level(self.position + side * 0.25) > 0.85)
            side = -side; // Pas vers le bord.
        return side;
    }
    if (battle.projectiles().empty()) {
        m_seenThreats.clear();
        m_ignoredThreats.clear();
    }
    return {};
}

Vec2 BotController::chooseSteering(const Battle &battle, int target)
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

    if (target < 0)
        return level > 0.3 ? toCenter * 0.5 : Vec2{};
    const Spinner &other = battle.spinner(target);
    Vec2 targetPos = other.position;
    if (m_skill.anticipate)
        targetPos += other.velocity * 0.25;
    const Vec2 toTarget = targetPos - self.position;
    const double distance = toTarget.length();

    switch (m_plan) {
    case Plan::Hunt: {
        if (!m_skill.tactics)
            return toTarget.normalized();
        // Se placer côté centre de la cible : les chocs la poussent vers le bord.
        const double targetLevel = geometry.level(targetPos);
        if (targetLevel < 0.2)
            return toTarget.normalized(); // Cible au centre : contact direct.
        const Vec2 outward = targetPos.normalized();
        const bool inside = (self.position - targetPos).dot(outward) < -0.04;
        if (inside || distance < 0.2)
            return toTarget.normalized();
        const Vec2 approach = targetPos - outward * 0.22;
        return (approach - self.position).normalized();
    }
    case Plan::Hold:
        if (m_skill.tactics && distance < 0.35) {
            // Face à une menace, se replier vers le centre en gardant l'adversaire côté bord.
            return (toCenter * 0.8 - toTarget.normalized() * 0.2).normalized();
        }
        return level > 0.25 ? toCenter * 0.7 : Vec2{};
    case Plan::Evade: {
        // Garder ses distances, en tournant autour du centre plutôt qu'en fonçant au bord.
        Vec2 flee = distance < 0.55 ? (-toTarget).normalized() : Vec2{};
        if (m_skill.tactics && flee.lengthSquared() > 0.0) {
            const Vec2 tangent = self.position.perpendicular().normalized();
            const double side = tangent.dot(flee) >= 0.0 ? 1.0 : -1.0;
            flee = (flee * 0.5 + tangent * side * 0.5).normalized();
        }
        const double safe = m_skill.tactics ? 0.4 : 0.5;
        Vec2 desired = flee * 0.8;
        if (level > safe)
            desired += toCenter * (level - safe) * 3.0;
        return desired.lengthSquared() > 1.0 ? desired.normalized() : desired;
    }
    }
    return {};
}

// ---------------------------------------------------------------------------
// Dash
// ---------------------------------------------------------------------------

int BotController::chooseDashTarget(const Battle &battle)
{
    std::bernoulli_distribution hesitate(m_skill.hesitate);
    if (hesitate(m_rng))
        return -1;

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

double BotController::dashValue(const Battle &battle, int target) const
{
    const BattleConfig &cfg = battle.config();
    const Spinner &self = battle.spinner(m_index);
    const Spinner &other = battle.spinner(target);
    const Vec2 direction = (other.position - self.position).normalized();

    // Choc prévu : on arrive à pleine vitesse de dash, bonus d'attaque actif.
    Spinner boosted = self;
    boosted.boostTimer = cfg.dashBoostDuration;
    boosted.velocity = direction * cfg.dashSpeed;
    const double approach = std::max(0.0, cfg.dashSpeed + other.velocity.dot(-direction));
    const double dealt = std::min(other.spin, battle.collisionDamage(boosted, other, approach));
    const double taken = battle.collisionDamage(other, boosted, approach);
    double value = dealt - taken - cfg.dashCost;

    // Éjection probable de la cible (poussée vers le bord).
    const double push = battle.knockback(boosted, other, approach);
    if (likelyEjection(battle, other.position, direction, push))
        value += other.spin * 0.7;
    if (dealt >= other.spin)
        value += 40.0; // Coup fatal.

    // Risque d'être soi-même renvoyé hors du stadium par le recul.
    const double recoil = battle.knockback(other, boosted, approach);
    const Vec2 contact = self.position + direction * std::min(0.35, (other.position - self.position).length());
    if (likelyEjection(battle, contact, -direction, recoil))
        value -= self.spin * 0.6;

    // Combos : une cible gelée ne peut ni fuir ni se placer.
    if (other.isSlowed())
        value += 6.0;
    return value;
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
    if (m_skill.anticipate) {
        const Vec2 predicted = other.position + other.velocity * (gap / cfg.dashSpeed);
        gap = std::min(gap, (predicted - self.position).length() - 2.0 * cfg.topRadius);
    }
    if (gap > reach * m_skill.rangeFactor)
        return false;

    // Ne pas se jeter soi-même hors du stadium (plus prudent sans rebord).
    const Vec2 landing = self.position + direction * (std::min(gap, reach) + 0.02);
    const double safeLevel = battle.stadium().hasRim ? 0.88 : 0.8;
    if (geometry.level(landing) > safeLevel)
        return false;

    const double opponentEdge = geometry.level(other.position);
    const double incoming = other.velocity.dot(-direction);
    // Alignement : > 0 si le dash pousse la cible vers le bord.
    const Vec2 outward = other.position.lengthSquared() > 1e-6 ? other.position.normalized()
                                                               : direction;
    const double alignment = direction.dot(outward);
    const double spinAfterDash = self.spin - cfg.dashCost;

    *score = m_skill.tactics ? opponentEdge + alignment + (1.0 - other.spinRatio()) - gap
                             : -distance;

    // Tactique : on ne dashe que si le dash rapporte plus qu'il ne coûte
    // (dégâts nets, éjection probable, coup fatal, combos).
    if (m_skill.tactics) {
        const double value = dashValue(battle, target);
        *score = value;
        const double threshold = m_plan == Plan::Hunt ? 0.0 : (m_plan == Plan::Hold ? 2.0 : 6.0);
        return spinAfterDash > 5.0 && value > threshold;
    }

    if (m_difficulty == Difficulty::Easy)
        return spinAfterDash > 10.0; // Débutant : dashe dès que possible.

    switch (m_plan) {
    case Plan::Hunt:
        if (m_skill.tactics && alignment < -0.3 && opponentEdge > 0.3)
            return false; // Mauvais angle : on la renverrait vers le centre.
        return spinAfterDash > 8.0;
    case Plan::Hold:
        return spinAfterDash > 15.0
            && ((opponentEdge > 0.55 && alignment > 0.2) || incoming > 0.5);
    case Plan::Evade:
        return spinAfterDash > 25.0 && opponentEdge > 0.7 && alignment > 0.5;
    }
    return false;
}

} // namespace toupie
