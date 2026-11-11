#include "game/MatchRules.h"

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace toupie {

namespace {
double clampMultiplier(double value)
{
    return std::clamp(value, MatchRules::kMinMultiplier, MatchRules::kMaxMultiplier);
}

bool same(double a, double b)
{
    return std::abs(a - b) < 1e-6;
}
} // namespace

BattleConfig MatchRules::apply(const BattleConfig &base) const
{
    BattleConfig c = base;
    const double v = std::max(0.1, speed);
    c.launchSpeed *= v;
    c.orbitBase *= v;
    c.orbitPerAttack *= v;
    c.dashSpeed *= v;
    c.steerAcceleration *= v * steering;

    c.knockbackBase *= knockback;
    c.knockbackPerAttack *= knockback;
    c.knockbackPerAttackerDefense *= knockback;
    c.knockbackApproach *= knockback;
    c.specialKnockbackScale *= knockback;

    c.passiveBase *= wear;
    c.damageBase *= damage;
    c.specialDamageScale *= damage;
    c.dashAttackMultiplier = 1.0 + (base.dashAttackMultiplier - 1.0) * dashPower;

    c.maxSpin = std::max(10.0, maxEndurance);
    c.maxDuration = std::max(0.0, timeLimit);

    c.specialsEnabled = base.specialsEnabled && specials;
    c.duelMode = duelMode;
    return c;
}

bool MatchRules::isDefault() const
{
    return *this == MatchRules{};
}

QString MatchRules::summary() const
{
    QStringList parts;
    auto percent = [&](const QString &label, double value) {
        if (!same(value, 1.0))
            parts << QCoreApplication::translate("toupie::MatchRules", "%1 %2 %").arg(label).arg(qRound(value * 100));
    };
    percent(QCoreApplication::translate("toupie::MatchRules", "Vitesse"), speed);
    percent(QCoreApplication::translate("toupie::MatchRules", "Rebonds"), knockback);
    percent(QCoreApplication::translate("toupie::MatchRules", "Usure"), wear);
    percent(QCoreApplication::translate("toupie::MatchRules", "Dégâts"), damage);
    percent(QCoreApplication::translate("toupie::MatchRules", "Dash"), dashPower);
    if (!same(maxEndurance, 100.0))
        parts << QCoreApplication::translate("toupie::MatchRules", "Endurance %1").arg(qRound(maxEndurance));
    if (timeLimit > 0.0)
        parts << QCoreApplication::translate("toupie::MatchRules", "Limite %1 s").arg(qRound(timeLimit));
    if (steering <= 0.0)
        parts << QCoreApplication::translate("toupie::MatchRules", "Sans pilotage");
    else
        percent(QCoreApplication::translate("toupie::MatchRules", "Pilotage"), steering);
    if (!specials)
        parts << QCoreApplication::translate("toupie::MatchRules", "Sans coups spéciaux");
    if (duelMode != s_defaultDuelMode) // Autre que celui des paramètres.
        parts << QCoreApplication::translate("toupie::MatchRules", "Duels : %1").arg(toString(duelMode));
    return parts.isEmpty() ? QCoreApplication::translate("toupie::MatchRules", "Règles normales") : parts.join(QStringLiteral(" · "));
}

QJsonObject MatchRules::toJson() const
{
    return {
        {"speed", speed},           {"knockback", knockback},
        {"wear", wear},             {"damage", damage},
        {"maxEndurance", maxEndurance}, {"timeLimit", timeLimit},
        {"steering", steering},     {"specials", specials},
        {"dashPower", dashPower},   {"duels", static_cast<int>(duelMode)},
    };
}

MatchRules MatchRules::fromJson(const QJsonObject &json)
{
    MatchRules r;
    r.speed = clampMultiplier(json.value("speed").toDouble(1.0));
    r.knockback = clampMultiplier(json.value("knockback").toDouble(1.0));
    r.wear = clampMultiplier(json.value("wear").toDouble(1.0));
    r.damage = clampMultiplier(json.value("damage").toDouble(1.0));
    r.maxEndurance = std::clamp(json.value("maxEndurance").toDouble(100.0), 10.0, 1000.0);
    r.timeLimit = std::clamp(json.value("timeLimit").toDouble(0.0), 0.0, 3600.0);
    r.steering = clampMultiplier(json.value("steering").toDouble(1.0));
    r.specials = json.value("specials").toBool(true);
    r.dashPower = clampMultiplier(json.value("dashPower").toDouble(1.0));
    // Pas le réglage local : un hôte qui n'envoie rien joue en timing.
    r.duelMode = static_cast<DuelMode>(std::clamp(json.value("duels").toInt(0), 0, 2));
    return r;
}

bool MatchRules::operator==(const MatchRules &o) const
{
    return same(speed, o.speed) && same(knockback, o.knockback) && same(wear, o.wear)
        && same(damage, o.damage) && same(maxEndurance, o.maxEndurance)
        && same(timeLimit, o.timeLimit) && same(steering, o.steering)
        && specials == o.specials && same(dashPower, o.dashPower) && duelMode == o.duelMode;
}

QString toString(DuelMode mode)
{
    switch (mode) {
    case DuelMode::Timing:
        return QCoreApplication::translate("toupie::MatchRules", "Timing");
    case DuelMode::Rush:
        return QCoreApplication::translate("toupie::MatchRules", "Rafale");
    case DuelMode::None:
        return QCoreApplication::translate("toupie::MatchRules", "Sans duel");
    }
    return {};
}

} // namespace toupie
