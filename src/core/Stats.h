#pragma once

#include <QString>

namespace toupie {

/// Statistiques d'une pièce ou d'une toupie complète.
struct Stats
{
    int attack = 0;
    int defense = 0;
    int endurance = 0;

    constexpr Stats operator+(const Stats &other) const
    {
        return {attack + other.attack, defense + other.defense, endurance + other.endurance};
    }

    constexpr Stats &operator+=(const Stats &other)
    {
        attack += other.attack;
        defense += other.defense;
        endurance += other.endurance;
        return *this;
    }

    constexpr bool operator==(const Stats &other) const
    {
        return attack == other.attack && defense == other.defense && endurance == other.endurance;
    }

    constexpr bool operator!=(const Stats &other) const { return !(*this == other); }

    constexpr int total() const { return attack + defense + endurance; }
};

/// Type d'une toupie, déduit de sa statistique dominante.
enum class TopType { Attack, Defense, Endurance, Balance };

/// Sens de rotation (vu du dessus).
enum class SpinDirection { Clockwise, CounterClockwise };

/// Type de la toupie : la stat strictement la plus élevée l'emporte,
/// en cas d'égalité sur la valeur maximale la toupie est de type « équilibre ».
TopType classify(const Stats &stats);

QString toString(TopType type);
QString toString(SpinDirection direction);

/// +1 pour le sens horaire, -1 pour le sens anti-horaire.
/// (Avec un repère écran où l'axe Y pointe vers le bas, un angle croissant
/// correspond au sens horaire.)
constexpr int spinSign(SpinDirection direction)
{
    return direction == SpinDirection::Clockwise ? 1 : -1;
}

} // namespace toupie
