#include "game/SpecialMove.h"

namespace toupie {

const QList<SpecialMove> &SpecialMove::all()
{
    static const QList<SpecialMove> moves = [] {
        using Shape = SpecialMove::Shape;
        using Effect = SpecialMove::Effect;
        QList<SpecialMove> list;

        SpecialMove wave;
        wave.id = QStringLiteral("onde");
        wave.name = QStringLiteral("Onde de choc");
        wave.description = QStringLiteral(
            "Anneau qui repousse violemment toutes les toupies proches. Idéal pour éjecter, "
            "mais peu de dégâts et une portée courte.");
        wave.shape = Shape::Ring;
        wave.damage = 4.0;
        wave.knockback = 1.1;
        wave.speed = 1.1;
        wave.range = 0.7;
        wave.width = 0.06;
        wave.color = 0x7fd6ff;
        list.append(wave);

        SpecialMove spear;
        spear.id = QStringLiteral("lance");
        spear.name = QStringLiteral("Lance d'acier");
        spear.description = QStringLiteral(
            "Trait fin et très rapide qui inflige de lourds dégâts à une cible. Presque aucun "
            "recul, et il faut viser juste.");
        spear.damage = 10.0;
        spear.knockback = 0.2;
        spear.speed = 2.0;
        spear.range = 1.8;
        spear.width = 0.05;
        spear.color = 0xe8eef5;
        list.append(spear);

        SpecialMove frost;
        frost.id = QStringLiteral("givre");
        frost.name = QStringLiteral("Souffle de givre");
        frost.description = QStringLiteral(
            "Vague glacée qui ralentit fortement la cible et l'empêche de piloter pendant "
            "3 secondes. Dégâts faibles.");
        frost.damage = 6.0;
        frost.knockback = 0.1;
        frost.speed = 1.3;
        frost.range = 1.6;
        frost.width = 0.11;
        frost.effect = Effect::Slow;
        frost.effectStrength = 0.7;
        frost.effectDuration = 3.5;
        frost.color = 0x9be7ff;
        list.append(frost);

        SpecialMove vortex;
        vortex.id = QStringLiteral("vortex");
        vortex.name = QStringLiteral("Vortex");
        vortex.description = QStringLiteral(
            "Anneau à longue portée qui aspire toutes les toupies vers le centre du stadium : "
            "provoque la mêlée, empêche de fuir. Presque aucun dégât.");
        vortex.shape = Shape::Ring;
        vortex.damage = 4.0;
        vortex.speed = 1.0;
        vortex.range = 1.3;
        vortex.width = 0.07;
        vortex.effect = Effect::PullToCenter;
        vortex.effectStrength = 0.9;
        vortex.color = 0xb58cff;
        list.append(vortex);

        SpecialMove warp;
        warp.id = QStringLiteral("distorsion");
        warp.name = QStringLiteral("Distorsion");
        warp.description = QStringLiteral(
            "Vague lente qui téléporte la cible près du bord du stadium. Aucun dégât, et "
            "facile à esquiver de loin.");
        warp.damage = 0.0;
        warp.speed = 1.15;
        warp.range = 1.6;
        warp.width = 0.1;
        warp.effect = Effect::Teleport;
        warp.effectStrength = 0.85; // Niveau (0 = centre, 1 = bord) d'arrivée.
        warp.color = 0xff7ad9;
        list.append(warp);

        SpecialMove siphon;
        siphon.id = QStringLiteral("siphon");
        siphon.name = QStringLiteral("Siphon");
        siphon.description = QStringLiteral(
            "Vague qui vole l'endurance de la cible : la toupie récupère tout ce qu'elle "
            "inflige. Dégâts moyens, recul faible.");
        siphon.damage = 7.0;
        siphon.knockback = 0.15;
        siphon.speed = 1.3;
        siphon.range = 1.4;
        siphon.width = 0.08;
        siphon.effect = Effect::Drain;
        siphon.effectStrength = 0.6;
        siphon.color = 0xff5c6c;
        list.append(siphon);

        SpecialMove quake;
        quake.id = QStringLiteral("seisme");
        quake.name = QStringLiteral("Séisme");
        quake.description = QStringLiteral(
            "Secousse rapide qui blesse toutes les toupies très proches et les empêche de "
            "dasher ou de lancer leur coup spécial pendant 3 secondes. Portée très courte.");
        quake.shape = Shape::Ring;
        quake.damage = 7.0;
        quake.knockback = 0.3;
        quake.speed = 1.6;
        quake.range = 0.5;
        quake.width = 0.07;
        quake.effect = Effect::Disarm;
        quake.effectDuration = 3.0;
        quake.color = 0xffb347;
        list.append(quake);

        return list;
    }();
    return moves;
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
