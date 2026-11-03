#include "core/Stats.h"

namespace toupie {

TopType classify(const Stats &stats)
{
    const int a = stats.attack;
    const int d = stats.defense;
    const int e = stats.endurance;

    if (a > d && a > e)
        return TopType::Attack;
    if (d > a && d > e)
        return TopType::Defense;
    if (e > a && e > d)
        return TopType::Endurance;
    return TopType::Balance;
}

QString toString(TopType type)
{
    switch (type) {
    case TopType::Attack:
        return QStringLiteral("Attaque");
    case TopType::Defense:
        return QStringLiteral("Défense");
    case TopType::Endurance:
        return QStringLiteral("Endurance");
    case TopType::Balance:
        return QStringLiteral("Équilibre");
    }
    return {};
}

QString toString(SpinDirection direction)
{
    return direction == SpinDirection::Clockwise ? QStringLiteral("Horaire")
                                                 : QStringLiteral("Anti-horaire");
}

} // namespace toupie
