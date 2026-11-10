#include "game/SpecialMove.h"

#include <QCoreApplication>

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
        wave.name = QCoreApplication::translate("toupie::SpecialMove", "Onde de choc");
        wave.description = QCoreApplication::translate(
            "toupie::SpecialMove",
            "Anneau qui repousse violemment toutes les toupies proches : idéal pour éjecter "
            "une cible déjà au bord. Dégâts faibles, portée courte.");
        wave.shape = Shape::Ring;
        wave.damage = 12.5;
        wave.knockback = 1.25;
        wave.speed = 1.2;
        wave.range = 0.75;
        wave.width = 0.06;
        wave.color = 0x34d6ff;
        list.append(wave);

        SpecialMove spear;
        spear.id = QStringLiteral("lance");
        spear.name = QCoreApplication::translate("toupie::SpecialMove", "Lance d'acier");
        spear.description = QCoreApplication::translate(
            "toupie::SpecialMove",
            "Trait fin qui inflige les plus lourds dégâts à une cible, sans autre effet. "
            "Presque aucun recul, et assez lent pour être esquivé de loin.");
        spear.damage = 24.0;
        spear.knockback = 0.3;
        spear.speed = 1.1;
        spear.range = 1.8;
        spear.width = 0.05;
        spear.color = 0xc9d8ff;
        list.append(spear);

        SpecialMove frost;
        frost.id = QStringLiteral("givre");
        frost.name = QCoreApplication::translate("toupie::SpecialMove", "Souffle de givre");
        frost.description = QCoreApplication::translate(
            "toupie::SpecialMove",
            "Vague glacée : pendant 3 secondes, la cible est fortement ralentie, ne peut plus "
            "piloter et le froid use sa rotation trois fois plus vite. Dégâts moyens.");
        frost.damage = 18.5;
        frost.knockback = 0.1;
        frost.speed = 1.0;
        frost.range = 1.6;
        frost.width = 0.11;
        frost.effect = Effect::Slow;
        frost.effectStrength = 0.95;
        frost.effectDuration = 3.1;
        frost.color = 0x8ff3ff;
        list.append(frost);

        SpecialMove vortex;
        vortex.id = QStringLiteral("vortex");
        vortex.name = QCoreApplication::translate("toupie::SpecialMove", "Vortex");
        vortex.description = QCoreApplication::translate(
            "toupie::SpecialMove",
            "Anneau à longue portée qui aspire violemment toutes les toupies vers le centre "
            "du stadium : provoque la mêlée, empêche de fuir. Dégâts modérés.");
        vortex.shape = Shape::Ring;
        vortex.damage = 17.0;
        vortex.speed = 1.0;
        vortex.range = 1.4;
        vortex.width = 0.07;
        vortex.effect = Effect::PullToCenter;
        vortex.effectStrength = 2.0;
        vortex.color = 0x8f5bff;
        list.append(vortex);

        SpecialMove warp;
        warp.id = QStringLiteral("distorsion");
        warp.name = QCoreApplication::translate("toupie::SpecialMove", "Distorsion");
        warp.description = QCoreApplication::translate(
            "toupie::SpecialMove",
            "Vague lente qui blesse la cible et la téléporte près du bord du stadium, lancée "
            "vers l'extérieur : elle risque l'éjection. Facile à esquiver de loin.");
        warp.damage = 23.0;
        warp.speed = 0.9;
        warp.range = 1.6;
        warp.width = 0.1;
        warp.effect = Effect::Teleport;
        warp.effectStrength = 0.868; // Niveau (0 = centre, 1 = bord) d'arrivée.
        warp.knockback = 0.5;       // Vitesse vers l'extérieur après téléportation.
        warp.color = 0xd24bff;
        list.append(warp);

        SpecialMove siphon;
        siphon.id = QStringLiteral("siphon");
        siphon.name = QCoreApplication::translate("toupie::SpecialMove", "Siphon");
        siphon.description = QCoreApplication::translate(
            "toupie::SpecialMove",
            "Vague qui vole l'endurance de la cible : la toupie récupère 60 % de ce qu'elle "
            "inflige. Dégâts modérés, recul faible.");
        siphon.damage = 12.0;
        siphon.knockback = 0.15;
        siphon.speed = 1.0;
        siphon.range = 1.4;
        siphon.width = 0.08;
        siphon.effect = Effect::Drain;
        siphon.effectStrength = 0.6;
        siphon.color = 0xff3b6b;
        list.append(siphon);

        SpecialMove quake;
        quake.id = QStringLiteral("seisme");
        quake.name = QCoreApplication::translate("toupie::SpecialMove", "Séisme");
        quake.description = QCoreApplication::translate(
            "toupie::SpecialMove",
            "Secousse qui blesse toutes les toupies très proches et les empêche de dasher "
            "ou de lancer leur coup spécial pendant 6 secondes. Portée très courte.");
        quake.shape = Shape::Ring;
        quake.damage = 18.0;
        quake.knockback = 0.5;
        quake.speed = 1.6;
        quake.range = 0.55;
        quake.width = 0.07;
        quake.effect = Effect::Disarm;
        quake.effectDuration = 6.0;
        quake.color = 0xb8ff2e;
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
