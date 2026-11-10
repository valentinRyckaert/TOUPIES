#include "core/Stats.h"

#include <QCoreApplication>

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
        return QCoreApplication::translate("toupie::Stats", "Attaque");
    case TopType::Defense:
        return QCoreApplication::translate("toupie::Stats", "Défense");
    case TopType::Endurance:
        return QCoreApplication::translate("toupie::Stats", "Endurance");
    case TopType::Balance:
        return QCoreApplication::translate("toupie::Stats", "Équilibre");
    }
    return {};
}

QString toString(SpinDirection direction)
{
    return direction == SpinDirection::Clockwise ? QCoreApplication::translate("toupie::Stats", "Horaire")
                                                 : QCoreApplication::translate("toupie::Stats", "Anti-horaire");
}

} // namespace toupie
