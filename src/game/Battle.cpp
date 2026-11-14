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

Battle::Battle(const QList<Top> &tops, const Stadium &stadium, const BattleConfig &config,
               std::uint32_t seed)
    : m_stadium(stadium)
    , m_config(stadium.apply(config))
    , m_geometry(stadium.geometry(m_config))
    , m_rng(seed)
{
    const int count = std::clamp(static_cast<int>(tops.size()), kMinPlayers, kMaxPlayers);
    for (int i = 0; i < count; ++i)
        m_spinners.push_back(makeSpinner(tops.value(i, tops.value(0)), m_config.maxSpin));
    m_pairCooldowns.assign(m_spinners.size() * m_spinners.size(), 0.0);
    for (int i = 0; i < count; ++i)
        m_spinners[i].position = startPosition(i);
}

Battle::Battle(const Top &first, const Top &second, const BattleConfig &config, std::uint32_t seed)
    : Battle(QList<Top>{first, second}, Stadium::classic(), config, seed)
{
}

Vec2 Battle::startPosition(int index) const
{
    // Joueurs répartis régulièrement autour du centre ; le joueur 0 à gauche.
    const double angle = kPi + index * 2.0 * kPi / playerCount();
    return Vec2::fromAngle(angle) * (m_config.startOffset * m_geometry.innerRadius());
}

void Battle::launch(const QList<double> &angles, const QList<double> &powers)
{
    if (m_phase != Phase::Ready)
        return;

    for (int i = 0; i < playerCount(); ++i) {
        Spinner &s = m_spinners[i];
        if (s.status == Spinner::Status::Forfeit)
            continue;
        const double power = std::clamp(powers.value(i, 1.0), m_config.launchPowerMin,
                                        m_config.launchPowerMax);
        s.position = startPosition(i);
        s.velocity = Vec2::fromAngle(angles.value(i, 0.0)) * (m_config.launchSpeed * power);
        s.status = Spinner::Status::Spinning;
    }
    m_phase = Phase::Running;
    checkEnd();
}

void Battle::launch(double firstAngle, double secondAngle)
{
    launch(QList<double>{firstAngle, secondAngle});
}

int Battle::nearestOpponent(int index) const
{
    int best = -1;
    double bestDistance = 1e18;
    for (int j = 0; j < playerCount(); ++j) {
        if (j == index || !m_spinners[j].isActive())
            continue;
        const double d = (m_spinners[j].position - m_spinners[index].position).lengthSquared();
        if (d < bestDistance) {
            bestDistance = d;
            best = j;
        }
    }
    return best;
}

bool Battle::canDash(int index) const
{
    if (index < 0 || index >= playerCount())
        return false;
    const Spinner &self = m_spinners[index];
    return m_phase == Phase::Running && self.isActive() && !self.isDashing()
        && !self.isDisarmed() && !self.isPulled() && !inDuel(index) && self.dashCooldown <= 0.0
        && self.spin > m_config.dashCost && nearestOpponent(index) >= 0;
}

void Battle::setSteering(int index, Vec2 direction)
{
    if (index < 0 || index >= playerCount())
        return;
    const double length = direction.length();
    m_spinners[index].steer = length > 1.0 ? direction / length : direction;
}

bool Battle::canSpecial(int index) const
{
    if (index < 0 || index >= playerCount() || !m_config.specialsEnabled)
        return false;
    const Spinner &self = m_spinners[index];
    return m_phase == Phase::Running && self.isActive() && !self.isDisarmed() && !inDuel(index)
        && !self.specialUsed && SpecialMove::find(self.top.special()).has_value()
        && nearestOpponent(index) >= 0;
}

bool Battle::requestSpecial(int index)
{
    if (!canSpecial(index))
        return false;
    Spinner &self = m_spinners[index];
    const SpecialMove move = *SpecialMove::find(self.top.special());
    const Spinner &target = m_spinners[nearestOpponent(index)];

    Projectile p;
    p.id = m_nextProjectileId++;
    p.move = move.id;
    p.owner = index;
    p.ring = move.shape == SpecialMove::Shape::Ring;
    p.origin = self.position;
    p.direction = (target.position - self.position).normalized();
    p.position = self.position + p.direction * m_config.topRadius;
    p.radius = m_config.topRadius;
    p.speed = move.speed;
    p.range = move.range;
    p.width = move.width;
    p.color = move.color;
    m_projectiles.push_back(p);
    self.specialUsed = true;
    self.spin *= 1.0 - m_config.specialCost;
    return true;
}

bool Battle::requestDash(int index, int targetIndex)
{
    if (const int side = m_duel.sideOf(index); side >= 0)
        return duelPress(index, m_duel.sides[side].cursor);
    if (!canDash(index))
        return false;
    if (targetIndex < 0 || targetIndex >= playerCount() || targetIndex == index
        || !m_spinners[targetIndex].isActive())
        targetIndex = nearestOpponent(index);

    Spinner &self = m_spinners[index];
    const Spinner &target = m_spinners[targetIndex];

    self.dashDirection = (target.position - self.position).normalized();
    self.velocity = self.dashDirection * m_config.dashSpeed;
    self.dashTimer = m_config.dashDuration;
    self.boostTimer = m_config.dashBoostDuration;
    self.dashCooldown = m_config.dashCooldown;
    self.spin -= m_config.dashCost;
    return true;
}

void Battle::applyImpulse(int index, Vec2 velocity)
{
    if (index >= 0 && index < playerCount() && m_spinners[index].isActive() && !inDuel(index))
        m_spinners[index].velocity += velocity;
}

void Battle::forfeit(int index)
{
    if (index < 0 || index >= playerCount() || m_phase == Phase::Finished)
        return;
    Spinner &s = m_spinners[index];
    if (s.status == Spinner::Status::Waiting) {
        s.status = Spinner::Status::Forfeit;
        m_eliminated.append(index);
    } else if (s.isActive()) {
        eliminate(index, Spinner::Status::Forfeit);
    }
    if (m_phase == Phase::Running)
        checkEnd();
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
    const double frost = s.isSlowed() ? 1.0 + m_config.slowWear * s.slowStrength : 1.0;
    return m_config.passiveBase * enduranceFactor * stability(s) * frost
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

double &Battle::pairCooldown(int i, int j)
{
    return m_pairCooldowns[static_cast<size_t>(i) * m_spinners.size() + static_cast<size_t>(j)];
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

void Battle::step(double h)
{
    for (int i = 0; i < playerCount(); ++i) {
        if (m_spinners[i].isActive() && !inDuel(i))
            integrate(m_spinners[i], h);
    }
    updateDuel(h);

    for (double &cooldown : m_pairCooldowns)
        cooldown = std::max(0.0, cooldown - h);
    for (int i = 0; i < playerCount(); ++i) {
        for (int j = i + 1; j < playerCount(); ++j)
            resolveCollision(i, j);
    }
    updateProjectiles(h);

    // Éliminations de ce pas, les plus faibles d'abord (départage du classement).
    QList<int> order;
    for (int i = 0; i < playerCount(); ++i) {
        if (m_spinners[i].isActive())
            order.append(i);
    }
    std::sort(order.begin(), order.end(),
              [this](int a, int b) { return m_spinners[a].spin < m_spinners[b].spin; });
    for (int i : order) {
        Spinner &s = m_spinners[i];
        applyRim(s);
        if (s.spin <= 0.0) {
            s.spin = 0.0;
            eliminate(i, Spinner::Status::Stopped);
        } else if (m_geometry.level(s.position) > 1.0) {
            eliminate(i, Spinner::Status::RingOut);
        }
    }

    m_elapsed += h;
    checkEnd();
}

void Battle::eliminate(int index, Spinner::Status status)
{
    Spinner &s = m_spinners[index];
    s.status = status;
    s.dashTimer = 0.0;
    m_eliminated.append(index);
    if (inDuel(index))
        m_duel.active = false; // Abandon pendant un duel.

    // Mêlée : la toupie qui a provoqué l'élimination récupère de l'endurance,
    // seulement si le combat continue (pas sur l'élimination finale).
    const auto active = std::count_if(m_spinners.begin(), m_spinners.end(),
                                      [](const Spinner &t) { return t.isActive(); });
    if (active >= 2 && status != Spinner::Status::Forfeit && s.lastHitBy >= 0
        && m_elapsed - s.lastHitTime <= m_config.knockoutWindow) {
        Spinner &striker = m_spinners[s.lastHitBy];
        if (striker.isActive())
            striker.spin = std::min(striker.maxSpin, striker.spin + m_config.knockoutReward);
    }
}

/// Avec rebord, une toupie qui atteint le bord lentement rebondit ; seule une
/// toupie lancée assez vite le franchit (et est éjectée quand son centre sort
/// du stadium). Sans rebord, rien ne la retient.
void Battle::applyRim(Spinner &s)
{
    if (!m_stadium.hasRim)
        return;

    if (m_geometry.level(s.position, m_config.topRadius) <= 1.0) {
        s.escaping = false;
        return;
    }
    if (s.escaping)
        return;

    const Vec2 normal = m_geometry.outwardNormal(s.position);
    const double outward = s.velocity.dot(normal);
    if (outward > m_config.rimEscapeSpeed) {
        s.escaping = true;
        return;
    }
    if (outward > 0.0)
        s.velocity -= normal * (outward * (1.0 + m_config.rimRestitution));
    if (outward > m_config.rimDamageThreshold) {
        // Percuter le rebord use un peu la rotation (la défense amortit le choc).
        const double mitigation = std::max(
            m_config.minReduction, 1.0 - m_config.damagePerDefense * s.stats.defense);
        const double loss = m_config.rimDamage * outward * mitigation * stability(s);
        s.spin -= loss;
        ImpactEvent event;
        event.point = s.position + normal * m_config.topRadius;
        event.intensity = std::min(1.0, loss / 2.0);
        event.first = event.second = -1; // Choc contre le rebord, sans adversaire.
        m_impacts.append(event);
    }
    s.position = m_geometry.projectInside(s.position, m_config.topRadius);
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
    } else if (s.isPulled()) {
        // Aspirée (vortex) : sa vitesse converge vers celle qui la ramène au centre,
        // rapide de loin, nulle au centre ; ni propulsion ni pilotage ne s'y opposent.
        const Vec2 target = s.position * -s.pullRate;
        s.velocity += (target - s.velocity) * (1.0 - std::exp(-m_config.pullGrip * h));
    } else {
        // Ralentie (givre) : moins de propulsion, plus de frottement, pas de pilotage.
        const double control = s.isSlowed() ? 1.0 - s.slowStrength : 1.0;

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
            acceleration += clockwiseTangent * (-s.spinSign * orbit * control);
            // Attraction vers le centre : sans action contraire, la toupie s'en rapproche.
            acceleration -= s.position / r * (m_config.centerPull * nearCenter);
        }

        // Pilotage aux flèches : légère correction de trajectoire.
        if (!s.isSlowed() && s.steer.lengthSquared() > 1e-9)
            acceleration += s.steer * m_config.steerAcceleration;

        const double friction = m_config.friction * (s.isSlowed() ? 1.0 + 2.0 * s.slowStrength : 1.0);
        acceleration -= s.velocity * friction;

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
    s.slowTimer = std::max(0.0, s.slowTimer - h);
    s.disarmTimer = std::max(0.0, s.disarmTimer - h);
    if (s.isPulled()) {
        s.pullTimer = std::max(0.0, s.pullTimer - h);
        if (!s.isPulled()) // Fin de l'aspiration : juste de quoi glisser jusqu'au centre.
            s.velocity = -s.position * m_config.friction;
    }
}

void Battle::resolveCollision(int i, int j)
{
    Spinner &a = m_spinners[i];
    Spinner &b = m_spinners[j];
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

    // Duellistes : immobiles, ils repoussent simplement les autres toupies.
    const bool aFrozen = inDuel(i);
    const bool bFrozen = inDuel(j);
    if (aFrozen && bFrozen)
        return;
    const double overlap = minDistance - distance;
    if (aFrozen || bFrozen) {
        Spinner &moving = aFrozen ? b : a;
        const Vec2 away = aFrozen ? n : -n;
        moving.position += away * overlap;
        const double toward = -moving.velocity.dot(away);
        if (toward > 0.0)
            moving.velocity += away * (toward * (1.0 + m_config.rimRestitution));
        return;
    }

    // Séparation des toupies (la plus lourde bouge moins).
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

    double &cooldown = pairCooldown(i, j);
    if (cooldown > 0.0)
        return; // Contact prolongé : un seul choc comptabilisé.

    const double approach = std::max(0.0, closing);
    if (a.isDashing() && b.isDashing() && !m_duel.active && m_config.duelMode != DuelMode::None) {
        startDuel(i, j, n, approach);
        return;
    }
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
    a.lastHitBy = j;
    b.lastHitBy = i;
    a.lastHitTime = b.lastHitTime = m_elapsed;

    // Un dash s'arrête au premier contact (le bonus d'attaque reste actif).
    a.dashTimer = 0.0;
    b.dashTimer = 0.0;

    cooldown = m_config.contactCooldown;

    ImpactEvent event;
    event.point = a.position + n * m_config.topRadius;
    event.intensity = std::min(1.0, (damageToA + damageToB) / 20.0);
    event.first = i;
    event.second = j;
    m_impacts.append(event);
}

// ---------------------------------------------------------------------------
// Coups spéciaux
// ---------------------------------------------------------------------------

namespace {
double distanceToSegment(Vec2 p, Vec2 a, Vec2 b)
{
    const Vec2 ab = b - a;
    const double len2 = ab.lengthSquared();
    const double t = len2 > 1e-12 ? std::clamp((p - a).dot(ab) / len2, 0.0, 1.0) : 0.0;
    return (p - (a + ab * t)).length();
}
} // namespace

void Battle::updateProjectiles(double h)
{
    for (Projectile &p : m_projectiles) {
        const auto move = SpecialMove::find(p.move);
        if (!move) {
            p.traveled = p.range + 1.0;
            continue;
        }
        if (p.ring) {
            // Anneau : touche chaque adversaire au passage du front.
            const double previous = p.radius;
            p.radius += p.speed * h;
            p.traveled = p.radius;
            for (int j = 0; j < playerCount(); ++j) {
                const Spinner &t = m_spinners[j];
                if (j == p.owner || !t.isActive() || inDuel(j) || p.hit.contains(j))
                    continue;
                const double d = (t.position - p.origin).length();
                if (d + m_config.topRadius >= previous - p.width
                    && d - m_config.topRadius <= p.radius + p.width && d <= p.range + m_config.topRadius) {
                    p.hit.append(j);
                    applySpecialHit(p, *move, j);
                }
            }
        } else {
            // Vague : avance en ligne droite et s'arrête sur le premier adversaire.
            const Vec2 previous = p.position;
            p.position += p.direction * (p.speed * h);
            p.traveled += p.speed * h;
            for (int j = 0; j < playerCount(); ++j) {
                const Spinner &t = m_spinners[j];
                if (j == p.owner || !t.isActive() || inDuel(j))
                    continue;
                if (distanceToSegment(t.position, previous, p.position)
                    <= m_config.topRadius + p.width) {
                    p.hit.append(j);
                    applySpecialHit(p, *move, j);
                    p.traveled = p.range + 1.0; // Absorbée.
                    break;
                }
            }
            if (m_geometry.level(p.position) > 1.2)
                p.traveled = p.range + 1.0;
        }
    }
    m_projectiles.erase(std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                                       [](const Projectile &p) { return p.traveled > p.range; }),
                        m_projectiles.end());
}

void Battle::applySpecialHit(const Projectile &projectile, const SpecialMove &move, int target)
{
    Spinner &t = m_spinners[target];
    Spinner &owner = m_spinners[projectile.owner];

    // Les stats comptent aussi face aux coups spéciaux : la défense réduit
    // les dégâts et le recul.
    const double mitigation = std::max(
        m_config.minReduction, 1.0 - m_config.damagePerDefense * t.stats.defense);
    const double damage = move.damage * m_config.specialDamageScale * mitigation * stability(t);
    t.spin -= damage;

    const Vec2 direction = projectile.ring ? (t.position - projectile.origin).normalized()
                                           : projectile.direction;
    // Le choc stoppe l'élan de la cible vers le lanceur, dash compris : un dash en cours
    // réimposait sa vitesse à chaque pas et effaçait le recul.
    t.dashTimer = 0.0;
    const double approach = t.velocity.dot(direction);
    if (approach < 0.0)
        t.velocity -= direction * approach;
    const double resistance = std::max(
        m_config.minReduction, 1.0 - m_config.knockbackPerDefense * t.stats.defense);
    t.velocity += direction * (move.knockback * m_config.specialKnockbackScale * resistance
                               * stability(t));

    switch (move.effect) {
    case SpecialMove::Effect::None:
        break;
    case SpecialMove::Effect::PullToCenter:
        // Aspiration pendant effectDuration : la distance au centre décroît
        // exponentiellement (taux effectStrength), sans jamais le dépasser.
        t.pullTimer = move.effectDuration;
        t.pullRate = move.effectStrength;
        t.velocity = -t.position * move.effectStrength;
        break;
    case SpecialMove::Effect::Slow:
        t.slowTimer = move.effectDuration;
        t.slowStrength = move.effectStrength;
        break;
    case SpecialMove::Effect::Teleport: {
        // Point aléatoire au niveau voulu (toutes les formes sont homogènes).
        std::uniform_real_distribution<double> angle(0.0, 2.0 * kPi);
        const Vec2 dir = Vec2::fromAngle(angle(m_rng));
        const double level = std::max(1e-6, m_geometry.level(dir));
        t.position = dir * (move.effectStrength / level);
        t.velocity = dir * (move.knockback * m_config.specialKnockbackScale);
        t.escaping = false;
        break;
    }
    case SpecialMove::Effect::Drain:
        if (owner.isActive())
            owner.spin = std::min(owner.maxSpin, owner.spin + damage * move.effectStrength);
        break;
    case SpecialMove::Effect::Disarm:
        t.disarmTimer = move.effectDuration;
        break;
    }

    t.lastHitBy = projectile.owner;
    t.lastHitTime = m_elapsed;

    ImpactEvent event;
    event.point = t.position;
    event.intensity = std::min(1.0, 0.4 + damage / 20.0);
    event.first = projectile.owner;
    event.second = target;
    event.special = true;
    event.color = move.color;
    m_impacts.append(event);
}

// ---------------------------------------------------------------------------
// Duels
// ---------------------------------------------------------------------------

void DuelSide::advance(double h)
{
    cursor += direction * speed * h;
    // Aller-retour : le curseur rebondit aux extrémités de la barre.
    while (cursor > 1.0 || cursor < -1.0) {
        cursor = cursor > 1.0 ? 2.0 - cursor : -2.0 - cursor;
        direction = -direction;
    }
}

void Battle::startDuel(int i, int j, Vec2 normal, double approach)
{
    m_duel = Duel{};
    m_duel.active = true;
    m_duel.first = i;
    m_duel.second = j;
    m_duel.normal = normal;
    m_duel.point = m_spinners[i].position + normal * m_config.topRadius;
    m_duel.approach = approach;
    for (DuelSide &side : m_duel.sides) {
        side.speed = m_config.duelCursorSpeed;
        side.zone = m_config.duelZoneStart;
    }
    for (int index : {i, j}) {
        Spinner &s = m_spinners[index];
        s.velocity = {};
        s.dashTimer = 0.0;
        s.lastHitBy = index == i ? j : i;
        s.lastHitTime = m_elapsed;
    }

    ImpactEvent event;
    event.point = m_duel.point;
    event.intensity = 1.0;
    event.first = i;
    event.second = j;
    m_impacts.append(event);
}

void Battle::updateDuel(double h)
{
    if (!m_duel.active)
        return;
    m_duel.elapsed += h;
    if (m_config.duelMode == DuelMode::Rush) {
        for (int index : {m_duel.first, m_duel.second}) { // Tournent sur place sans s'user.
            Spinner &s = m_spinners[index];
            s.angle = std::fmod(s.angle + s.spinSign * (6.0 + 30.0 * s.spinRatio()) * h, 2.0 * kPi);
        }
        const int rope = m_duel.rope();
        if (std::abs(rope) >= m_config.rushLimit || m_duel.elapsed >= m_config.rushDuration)
            resolveDuel(rope == 0 ? -1 : rope > 0 ? 1 : 0); // Pile au milieu : nul.
        return;
    }
    bool timedOut[2] = {false, false};
    for (int k = 0; k < 2; ++k) {
        DuelSide &side = m_duel.sides[k];
        // Les duellistes tournent sur place (affichage) sans s'user.
        Spinner &s = m_spinners[k == 0 ? m_duel.first : m_duel.second];
        s.angle = std::fmod(s.angle + s.spinSign * (6.0 + 30.0 * s.spinRatio()) * h, 2.0 * kPi);
        if (side.done)
            continue;
        side.advance(h);
        side.idle += h;
        timedOut[k] = side.idle > m_config.duelTimeout;
    }
    if (timedOut[0] && timedOut[1])
        resolveDuel(-1);
    else if (timedOut[0] || timedOut[1])
        resolveDuel(timedOut[0] ? 0 : 1);
}

bool Battle::duelPress(int index, double cursor)
{
    const int k = m_duel.sideOf(index);
    if (k < 0 || m_phase != Phase::Running || m_duel.sides[k].done)
        return false;
    DuelSide &side = m_duel.sides[k];
    side.idle = 0.0;
    if (m_config.duelMode == DuelMode::Rush) {
        ++side.presses; // La fin est décidée par updateDuel (durée ou avance décisive).
        return true;
    }
    if (std::abs(std::clamp(cursor, -1.0, 1.0)) > side.zone) {
        resolveDuel(k);
        return true;
    }
    if (side.zone <= m_config.duelZoneMin + 1e-9) {
        side.done = ++side.streak >= m_config.duelStreak;
    } else {
        side.zone = std::max(m_config.duelZoneMin, side.zone * m_config.duelZoneShrink);
    }
    side.speed *= m_config.duelSpeedGain;
    if (m_duel.sides[0].done && m_duel.sides[1].done)
        resolveDuel(-1);
    return true;
}

void Battle::resolveDuel(int loserSide)
{
    const Duel duel = m_duel;
    m_duel.active = false;
    Spinner &a = m_spinners[duel.first];
    Spinner &b = m_spinners[duel.second];
    const Vec2 n = duel.normal;

    ImpactEvent event;
    event.point = duel.point;
    event.first = duel.first;
    event.second = duel.second;
    if (loserSide < 0) {
        // Nul : les toupies se repoussent sans dégâts.
        a.velocity -= n * knockback(b, a, duel.approach);
        b.velocity += n * knockback(a, b, duel.approach);
        event.intensity = 0.6;
        event.duelLoser = -1;
    } else {
        Spinner &loser = loserSide == 0 ? a : b;
        Spinner winner = loserSide == 0 ? b : a;
        winner.boostTimer = std::max(winner.boostTimer, 1e-3); // Dégâts d'un dash.
        const double damage = m_config.duelDamageMultiplier
                            * collisionDamage(winner, loser, duel.approach);
        loser.spin -= damage;
        const Vec2 away = loserSide == 0 ? -n : n;
        loser.velocity += away * knockback(winner, loser, duel.approach);
        event.intensity = std::min(1.0, 0.4 + damage / 20.0);
        event.duelLoser = loserSide == 0 ? duel.first : duel.second;
    }
    a.boostTimer = b.boostTimer = 0.0;
    pairCooldown(std::min(duel.first, duel.second), std::max(duel.first, duel.second))
        = m_config.contactCooldown;
    m_impacts.append(event);
}

void Battle::checkEnd()
{
    if (m_phase != Phase::Running)
        return;

    QList<int> active;
    for (int i = 0; i < playerCount(); ++i) {
        if (m_spinners[i].isActive())
            active.append(i);
    }

    auto reasonFor = [](const Spinner &loser) {
        switch (loser.status) {
        case Spinner::Status::RingOut:
            return EndReason::RingOut;
        case Spinner::Status::Forfeit:
            return EndReason::Forfeit;
        default:
            return EndReason::SpinOut;
        }
    };

    if (active.size() == 1) {
        const int last = m_eliminated.isEmpty() ? active.first() : m_eliminated.last();
        finish(active.first(), reasonFor(m_spinners[last]));
    } else if (active.isEmpty()) {
        // Toutes éliminées au même pas : la dernière éliminée (la plus
        // d'endurance) gagne, sauf égalité parfaite.
        const int last = m_eliminated.last();
        const int previous = m_eliminated.size() > 1 ? m_eliminated.at(m_eliminated.size() - 2)
                                                     : -1;
        const bool tie = previous >= 0
                      && std::abs(m_spinners[last].spin - m_spinners[previous].spin) < 1e-9;
        finish(tie ? -1 : last, reasonFor(m_spinners[last]));
    } else if (m_config.maxDuration > 0.0 && m_elapsed >= m_config.maxDuration) {
        std::sort(active.begin(), active.end(),
                  [this](int a, int b) { return m_spinners[a].spin > m_spinners[b].spin; });
        const bool tie = std::abs(m_spinners[active[0]].spin - m_spinners[active[1]].spin) < 0.5;
        finish(tie ? -1 : active.first(), EndReason::Timeout);
    }
}

void Battle::finish(int winner, EndReason reason)
{
    m_phase = Phase::Finished;
    m_winner = winner;
    m_endReason = reason;
    m_duel.active = false;
    for (Spinner &s : m_spinners)
        s.dashTimer = 0.0;

    // Classement : toupies encore en jeu (par endurance), puis éliminées de la
    // plus tardive à la plus précoce.
    m_ranking.clear();
    QList<int> active;
    for (int i = 0; i < playerCount(); ++i) {
        if (m_spinners[i].isActive())
            active.append(i);
    }
    std::sort(active.begin(), active.end(),
              [this](int a, int b) { return m_spinners[a].spin > m_spinners[b].spin; });
    m_ranking = active;
    for (auto it = m_eliminated.crbegin(); it != m_eliminated.crend(); ++it) {
        if (!m_ranking.contains(*it))
            m_ranking.append(*it);
    }
    if (winner >= 0 && !m_ranking.isEmpty() && m_ranking.first() != winner) {
        m_ranking.removeAll(winner);
        m_ranking.prepend(winner);
    }
}

} // namespace toupie
