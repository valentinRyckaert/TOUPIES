#include "game/SpecialMove.h"

namespace toupie {

const QList<SpecialMove> &SpecialMove::all()
{
    return registry();
}

QList<SpecialMove> &SpecialMove::registry()
{
    static QList<SpecialMove> moves = [] {
        using Shape = SpecialMove::Shape;
        using Effect = SpecialMove::Effect;
        QList<SpecialMove> list;

        SpecialMove wave;
        wave.id = QStringLiteral("onde");
        wave.name = QStringLiteral("Onde de choc");
        wave.description = QStringLiteral(
            "Anneau qui repousse violemment toutes les toupies proches : idéal pour éjecter "
            "une cible déjà au bord. Dégâts moyens, portée courte.");
        wave.shape = Shape::Ring;
        wave.damage = 16.0;
        wave.knockback = 1.25;
        wave.speed = 1.2;
        wave.range = 0.75;
        wave.width = 0.06;
        wave.color = 0x7fd6ff;
        list.append(wave);

        SpecialMove spear;
        spear.id = QStringLiteral("lance");
        spear.name = QStringLiteral("Lance d'acier");
        spear.description = QStringLiteral(
            "Trait fin qui inflige de très lourds dégâts à une cible. Presque aucun recul, "
            "et assez lent pour être esquivé de loin.");
        spear.damage = 42.0;
        spear.knockback = 0.3;
        spear.speed = 1.4;
        spear.range = 1.8;
        spear.width = 0.05;
        spear.color = 0xe8eef5;
        list.append(spear);

        SpecialMove frost;
        frost.id = QStringLiteral("givre");
        frost.name = QStringLiteral("Souffle de givre");
        frost.description = QStringLiteral(
            "Vague glacée qui inflige des dégâts moyens, ralentit fortement la cible et "
            "l'empêche de piloter pendant 3 secondes.");
        frost.damage = 23.5;
        frost.knockback = 0.1;
        frost.speed = 1.0;
        frost.range = 1.6;
        frost.width = 0.11;
        frost.effect = Effect::Slow;
        frost.effectStrength = 0.95;
        frost.effectDuration = 3.1;
        frost.color = 0x9be7ff;
        list.append(frost);

        SpecialMove vortex;
        vortex.id = QStringLiteral("vortex");
        vortex.name = QStringLiteral("Vortex");
        vortex.description = QStringLiteral(
            "Anneau à longue portée qui aspire toutes les toupies vers le centre du stadium "
            ": provoque la mêlée, empêche de fuir. Dégâts modérés.");
        vortex.shape = Shape::Ring;
        vortex.damage = 13.0;
        vortex.speed = 1.0;
        vortex.range = 1.4;
        vortex.width = 0.07;
        vortex.effect = Effect::PullToCenter;
        vortex.effectStrength = 1.16;
        vortex.color = 0xb58cff;
        list.append(vortex);

        SpecialMove warp;
        warp.id = QStringLiteral("distorsion");
        warp.name = QStringLiteral("Distorsion");
        warp.description = QStringLiteral(
            "Vague lente qui blesse la cible et la téléporte près du bord du stadium. "
            "Facile à esquiver de loin.");
        warp.damage = 20.0;
        warp.speed = 0.9;
        warp.range = 1.6;
        warp.width = 0.1;
        warp.effect = Effect::Teleport;
        warp.effectStrength = 0.868; // Niveau (0 = centre, 1 = bord) d'arrivée.
        warp.knockback = 0.14;      // Vitesse vers l'extérieur après téléportation.
        warp.color = 0xff7ad9;
        list.append(warp);

        SpecialMove siphon;
        siphon.id = QStringLiteral("siphon");
        siphon.name = QStringLiteral("Siphon");
        siphon.description = QStringLiteral(
            "Vague qui vole l'endurance de la cible : la toupie récupère tout ce qu'elle "
            "inflige. Dégâts moyens, recul faible.");
        siphon.damage = 17.0;
        siphon.knockback = 0.15;
        siphon.speed = 1.0;
        siphon.range = 1.4;
        siphon.width = 0.08;
        siphon.effect = Effect::Drain;
        siphon.effectStrength = 1.0;
        siphon.color = 0xff5c6c;
        list.append(siphon);

        SpecialMove quake;
        quake.id = QStringLiteral("seisme");
        quake.name = QStringLiteral("Séisme");
        quake.description = QStringLiteral(
            "Secousse qui blesse toutes les toupies très proches et les empêche de dasher "
            "ou de lancer leur coup spécial pendant 4,5 secondes. Portée très courte.");
        quake.shape = Shape::Ring;
        quake.damage = 15.0;
        quake.knockback = 0.5;
        quake.speed = 1.6;
        quake.range = 0.55;
        quake.width = 0.07;
        quake.effect = Effect::Disarm;
        quake.effectDuration = 4.5;
        quake.color = 0xffb347;
        list.append(quake);

        return list;
    }();
    return moves;
}

bool SpecialMove::overrideStat(const QString &id, const QString &field, double value)
{
    for (SpecialMove &move : registry()) {
        if (move.id != id)
            continue;
        if (field == QLatin1String("damage"))
            move.damage = value;
        else if (field == QLatin1String("knockback"))
            move.knockback = value;
        else if (field == QLatin1String("speed"))
            move.speed = value;
        else if (field == QLatin1String("range"))
            move.range = value;
        else if (field == QLatin1String("width"))
            move.width = value;
        else if (field == QLatin1String("effectStrength"))
            move.effectStrength = value;
        else if (field == QLatin1String("effectDuration"))
            move.effectDuration = value;
        else
            return false;
        return true;
    }
    return false;
}

std::optional<SpecialMove> SpecialMove::find(const QString &id)
{
    for (const SpecialMove &move : all()) {
        if (move.id == id)
            return move;
    }
    return std::nullopt;
}

} // namespace toupie
