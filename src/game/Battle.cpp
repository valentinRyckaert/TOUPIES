#include "game/Battle.h"

#include <algorithm>
#include <cmath>

namespace toupie {

namespace {
constexpr double kPi = 3.14159265358979323846;

Spinner makeSpinner(const Top &top, double maxSpin)
{
    Spinner s;
    s.top = top;
    s.stats = top.stats();
    s.spinSign = spinSign(top.spin());
    s.maxSpin = maxSpin;
    s.spin = maxSpin;
    return s;
}
} // namespace

Battle::Battle(const Top &first, const Top &second, const BattleConfig &config, std::uint32_t seed)
    : m_config(config)
    , m_spinners{makeSpinner(first, config.maxSpin), makeSpinner(second, config.maxSpin)}
    , m_rng(seed)
{
    for (int i = 0; i < 2; ++i)
        m_spinners[i].position = startPosition(i);
}

Vec2 Battle::startPosition(int index) const
{
    return {index == 0 ? -m_config.startOffset : m_config.startOffset, 0.0};
}

void Battle::launch(double firstAngle, double secondAngle)
{
    if (m_phase != Phase::Ready)
        return;

    const double angles[2] = {firstAngle, secondAngle};
    for (int i = 0; i < 2; ++i) {
        Spinner &s = m_spinners[i];
        s.position = startPosition(i);
        s.velocity = Vec2::fromAngle(angles[i]) * m_config.launchSpeed;
        s.status = Spinner::Status::Spinning;
    }
    m_phase = Phase::Running;
}

bool Battle::canDash(int index) const
{
    const Spinner &self = m_spinners.at(index);
    const Spinner &other = m_spinners.at(1 - index);
    return m_phase == Phase::Running && self.isActive() && other.isActive() && !self.isDashing()
        && self.dashCooldown <= 0.0 && self.spin > m_config.dashCost;
}

bool Battle::requestDash(int index)
{
    if (!canDash(index))
        return false;

    Spinner &self = m_spinners[index];
    const Spinner &other = m_spinners[1 - index];

    self.dashDirection = (other.position - self.position).normalized();
    self.velocity = self.dashDirection * m_config.dashSpeed;
    self.dashTimer = m_config.dashDuration;
    self.boostTimer = m_config.dashBoostDuration;
    self.dashCooldown = m_config.dashCooldown;
    self.spin -= m_config.dashCost;
    return true;
}

void Battle::advance(double dt)
{
    if (m_phase != Phase::Running)
        return;

    m_accumulator += std::min(dt, 0.1);
    while (m_accumulator >= m_config.fixedStep && m_phase == Phase::Running) {
        step(m_config.fixedStep);
        m_accumulator -= m_config.fixedStep;
    }
}

QList<ImpactEvent> Battle::takeImpacts()
{
    QList<ImpactEvent> impacts;
    impacts.swap(m_impacts);
    return impacts;
}

// ---------------------------------------------------------------------------
// Formules
// ---------------------------------------------------------------------------

/// Perte d'endurance naturelle : plus la stat d'endurance est élevée, plus la
/// perte est faible. Se déplacer vite use aussi la rotation.
double Battle::passiveLossPerSecond(const Spinner &s) const
{
    const double enduranceFactor = std::max(
        m_config.minReduction, 1.0 - m_config.passivePerEndurance * s.stats.endurance);
    return m_config.passiveBase * enduranceFactor * stability(s)
         * (1.0 + m_config.speedDrain * s.velocity.length());
}

/// Attaque effective : bonus pendant un dash, et une toupie qui tourne
/// lentement frappe moins fort.
double Battle::effectiveAttack(const Spinner &s) const
{
    const double boost = s.isBoosted() ? m_config.dashAttackMultiplier : 1.0;
    return s.stats.attack * boost * (0.6 + 0.4 * s.spinRatio());
}

/// Endurance perdue par `target` lorsqu'elle est frappée par `attacker`.
double Battle::collisionDamage(const Spinner &attacker, const Spinner &target,
                               double approachSpeed) const
{
    const double impact = std::min(m_config.maxImpact,
                                   m_config.minImpact + approachSpeed / m_config.impactRef);
    const double mitigation = std::max(
        m_config.minReduction, 1.0 - m_config.damagePerDefense * target.stats.defense);
    const double spinFactor
        = attacker.spinSign != target.spinSign ? m_config.oppositeSpinDamage : 1.0;
    return m_config.damageBase * effectiveAttack(attacker) * impact * mitigation * spinFactor
         * stability(target);
}

/// Vitesse de recul imprimée à `target` : la défense réduit le recul.
double Battle::knockback(const Spinner &attacker, const Spinner &target, double approachSpeed) const
{
    const double push = m_config.knockbackBase
                      + m_config.knockbackPerAttack * effectiveAttack(attacker)
                      + m_config.knockbackPerAttackerDefense * attacker.stats.defense
                      + m_config.knockbackApproach * approachSpeed;
    const double spinFactor
        = attacker.spinSign == target.spinSign ? m_config.sameSpinKnockback : 1.0;
    const double resistance = std::max(
        m_config.minReduction, 1.0 - m_config.knockbackPerDefense * target.stats.defense);
    return push * spinFactor * resistance * stability(target);
}

/// Bonus du type équilibre : sans stat dominante, la toupie est plus stable
/// (moins d'usure, de dégâts et de recul).
double Battle::stability(const Spinner &s) const
{
    return classify(s.stats) == TopType::Balance ? 1.0 - m_config.balanceStability : 1.0;
}

double Battle::mass(const Spinner &s) const
{
    return 1.0 + m_config.massPerDefense * s.stats.defense;
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

void Battle::step(double h)
{
    for (Spinner &s : m_spinners) {
        if (s.isActive())
            integrate(s, h);
    }

    m_contactCooldown = std::max(0.0, m_contactCooldown - h);
    resolveCollision();

    for (Spinner &s : m_spinners) {
        if (!s.isActive())
            continue;
        applyRim(s);
        if (s.spin <= 0.0) {
            s.spin = 0.0;
            s.status = Spinner::Status::Stopped;
        } else if (s.position.length() > m_config.stadiumRadius) {
            s.status = Spinner::Status::RingOut;
        }
    }

    m_elapsed += h;
    checkEnd();
}

/// Le bord du stadium est un rebord : une toupie qui l'atteint lentement
/// rebondit, seule une toupie lancée assez vite le franchit (et est éjectée
/// quand son centre sort du cercle).
void Battle::applyRim(Spinner &s)
{
    const double r = s.position.length();
    const double rimRadius = m_config.stadiumRadius - m_config.topRadius;
    if (r <= rimRadius) {
        s.escaping = false;
        return;
    }
    if (s.escaping)
        return;

    const Vec2 normal = s.position / r;
    const double outward = s.velocity.dot(normal);
    if (outward > m_config.rimEscapeSpeed) {
        s.escaping = true;
        return;
    }
    if (outward > 0.0)
        s.velocity -= normal * (outward * (1.0 + m_config.rimRestitution));
    s.position = normal * rimRadius;
}

void Battle::integrate(Spinner &s, double h)
{
    const double ratio = s.spinRatio();

    if (s.isDashing()) {
        // Déplacement rapide et rectiligne vers la cible.
        s.velocity = s.dashDirection * m_config.dashSpeed;
        s.dashTimer -= h;
        if (s.dashTimer <= 0.0) {
            s.dashTimer = 0.0;
            s.velocity = s.dashDirection * m_config.postDashSpeed;
        }
    } else {
        // Pente de la cuvette : ramène la toupie vers le centre.
        Vec2 acceleration = s.position * -m_config.slopeStrength;

        // Propulsion due à la rotation : la toupie parcourt le stadium dans
        // le sens inverse de sa rotation. perpendicular() donne la tangente
        // horaire, on prend donc l'opposé pour une toupie horaire.
        const double r = s.position.length();
        if (r > 1e-6) {
            const Vec2 clockwiseTangent = s.position.perpendicular() / r;
            const double nearCenter = std::min(1.0, r / 0.15);
            const double orbit = (m_config.orbitBase + m_config.orbitPerAttack * s.stats.attack)
                               * (0.4 + 0.6 * ratio) * nearCenter;
            acceleration += clockwiseTangent * (-s.spinSign * orbit);
        }

        acceleration -= s.velocity * m_config.friction;

        // Une toupie à bout de souffle vacille.
        if (ratio < m_config.wobbleThreshold) {
            std::uniform_real_distribution<double> angle(0.0, 2.0 * kPi);
            const double strength
                = m_config.wobbleStrength * (1.0 - ratio / m_config.wobbleThreshold);
            acceleration += Vec2::fromAngle(angle(m_rng)) * strength;
        }

        s.velocity += acceleration * h;
    }

    s.position += s.velocity * h;
    s.angle = std::fmod(s.angle + s.spinSign * (6.0 + 30.0 * ratio) * h, 2.0 * kPi);
    s.spin -= passiveLossPerSecond(s) * h;

    s.dashCooldown = std::max(0.0, s.dashCooldown - h);
    s.boostTimer = std::max(0.0, s.boostTimer - h);
}

void Battle::resolveCollision()
{
    Spinner &a = m_spinners[0];
    Spinner &b = m_spinners[1];
    if (!a.isActive() || !b.isActive())
        return;

    const Vec2 delta = b.position - a.position;
    const double distance = delta.length();
    const double minDistance = 2.0 * m_config.topRadius;
    if (distance >= minDistance)
        return;

    const Vec2 n = distance > 1e-9 ? delta / distance : Vec2{1.0, 0.0};
    const double ma = mass(a);
    const double mb = mass(b);
    const double total = ma + mb;

    // Séparation des toupies (la plus lourde bouge moins).
    const double overlap = minDistance - distance;
    a.position -= n * (overlap * mb / total);
    b.position += n * (overlap * ma / total);

    // Le choc absorbe la vitesse de rapprochement de chaque toupie : c'est
    // ensuite le recul (fonction des stats) qui détermine la séparation.
    const double closing = (a.velocity - b.velocity).dot(n);
    const double aTowardB = a.velocity.dot(n);
    const double bTowardA = -b.velocity.dot(n);
    if (aTowardB > 0.0)
        a.velocity -= n * aTowardB;
    if (bTowardA > 0.0)
        b.velocity += n * bTowardA;

    if (m_contactCooldown > 0.0)
        return; // Contact prolongé : un seul choc comptabilisé.

    const double approach = std::max(0.0, closing);
    const double damageToA = collisionDamage(b, a, approach);
    const double damageToB = collisionDamage(a, b, approach);
    const double pushA = knockback(b, a, approach);
    const double pushB = knockback(a, b, approach);

    a.velocity -= n * pushA;
    b.velocity += n * pushB;

    // Déviation latérale : la surface de chaque toupie « entraîne » l'autre.
    // Deux toupies de même sens se repoussent latéralement, deux toupies de
    // sens opposés s'annulent partiellement.
    const Vec2 t = n.perpendicular();
    const double deflection = m_config.spinDeflection * (0.5 + approach);
    const double fromA = a.spinSign * a.spinRatio() * deflection;
    const double fromB = b.spinSign * b.spinRatio() * deflection;
    b.velocity += t * fromA;
    a.velocity -= t * fromA;
    a.velocity -= t * fromB;
    b.velocity += t * fromB;

    a.spin -= damageToA;
    b.spin -= damageToB;

    // Un dash s'arrête au premier contact (le bonus d'attaque reste actif).
    a.dashTimer = 0.0;
    b.dashTimer = 0.0;

    m_contactCooldown = m_config.contactCooldown;

    ImpactEvent event;
    event.point = a.position + n * m_config.topRadius;
    event.intensity = std::min(1.0, (damageToA + damageToB) / 20.0);
    event.damage = {damageToA, damageToB};
    m_impacts.append(event);
}

void Battle::checkEnd()
{
    if (m_phase != Phase::Running)
        return;

    const Spinner &a = m_spinners[0];
    const Spinner &b = m_spinners[1];
    const bool aLost = !a.isActive();
    const bool bLost = !b.isActive();

    auto reasonFor = [](const Spinner &loser) {
        return loser.status == Spinner::Status::RingOut ? EndReason::RingOut : EndReason::SpinOut;
    };

    if (aLost && bLost) {
        const EndReason reason = (a.status == Spinner::Status::RingOut
                                  || b.status == Spinner::Status::RingOut)
                                   ? EndReason::RingOut
                                   : EndReason::SpinOut;
        if (std::abs(a.spin - b.spin) < 1e-9)
            finish(-1, reason);
        else
            finish(a.spin > b.spin ? 0 : 1, reason);
    } else if (aLost) {
        finish(1, reasonFor(a));
    } else if (bLost) {
        finish(0, reasonFor(b));
    } else if (m_elapsed >= m_config.maxDuration) {
        if (std::abs(a.spin - b.spin) < 0.5)
            finish(-1, EndReason::Timeout);
        else
            finish(a.spin > b.spin ? 0 : 1, EndReason::Timeout);
    }
}

void Battle::finish(int winner, EndReason reason)
{
    m_phase = Phase::Finished;
    m_winner = winner;
    m_endReason = reason;
    for (Spinner &s : m_spinners)
        s.dashTimer = 0.0;
}

} // namespace toupie
