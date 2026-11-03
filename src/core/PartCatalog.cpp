#include "core/PartCatalog.h"

#include <algorithm>
#include <utility>

namespace toupie {

namespace {

Part wheel(const char *id, const char *name, Stats stats, SpinDirection spin, int hue)
{
    return {QString::fromLatin1(id), QString::fromUtf8(name), PartKind::Wheel, stats, spin, hue, {}};
}

Part middle(const char *id, const char *name, Stats stats, int hue)
{
    return {QString::fromLatin1(id), QString::fromUtf8(name), PartKind::Middle, stats,
            SpinDirection::Clockwise, hue, {}};
}

Part bolt(const char *id, const char *name, const char *special, int hue)
{
    Part p{QString::fromLatin1(id), QString::fromUtf8(name), PartKind::Bolt, Stats{},
           SpinDirection::Clockwise, hue, {}};
    p.special = QString::fromLatin1(special);
    return p;
}

Part tip(const char *id, const char *name, Stats stats, int hue)
{
    return {QString::fromLatin1(id), QString::fromUtf8(name), PartKind::Tip, stats,
            SpinDirection::Clockwise, hue, {}};
}

QList<Part> defaultParts()
{
    using SD = SpinDirection;
    return {
        // Roues métalliques (budget 10) : elles fixent le sens de rotation.
        wheel("RM1", "Rempart", {3, 5, 2}, SD::Clockwise, 210),
        wheel("RM2", "Lame Cyclone", {6, 2, 2}, SD::Clockwise, 0),
        wheel("RM3", "Bastion", {2, 6, 2}, SD::CounterClockwise, 120),
        wheel("RM4", "Éclipse", {2, 2, 6}, SD::CounterClockwise, 275),
        wheel("RM5", "Prisme", {4, 3, 3}, SD::Clockwise, 45),
        wheel("RM6", "Tonnerre", {5, 3, 2}, SD::CounterClockwise, 25),
        wheel("RM7", "Ouragan", {2, 3, 5}, SD::Clockwise, 190),
        wheel("RM8", "Orbite", {4, 2, 4}, SD::CounterClockwise, 320),
        wheel("RM9", "Météore", {7, 2, 1}, SD::Clockwise, 15),
        wheel("RM10", "Titan", {1, 7, 2}, SD::CounterClockwise, 100),
        wheel("RM11", "Nébuleuse", {1, 2, 7}, SD::Clockwise, 250),
        wheel("RM12", "Faucon", {5, 2, 3}, SD::Clockwise, 340),
        wheel("RM13", "Zéphyr", {3, 2, 5}, SD::CounterClockwise, 170),
        wheel("RM14", "Golem", {3, 6, 1}, SD::Clockwise, 80),

        // Parties intermédiaires (budget 6).
        middle("PI1", "Noyau", {2, 3, 1}, 200),
        middle("PI2", "Griffe", {4, 1, 1}, 10),
        middle("PI3", "Carapace", {1, 4, 1}, 130),
        middle("PI4", "Anneau", {1, 1, 4}, 280),
        middle("PI5", "Axe", {2, 2, 2}, 50),
        middle("PI6", "Turbine", {3, 1, 2}, 30),
        middle("PI7", "Lame", {3, 2, 1}, 0),
        middle("PI8", "Bouclier", {1, 3, 2}, 140),
        middle("PI9", "Spirale", {2, 1, 3}, 300),
        middle("PI10", "Contrepoids", {1, 2, 3}, 230),

        // Pointes (budget 7).
        tip("P1", "Pivot", {1, 4, 2}, 220),
        tip("P2", "Gomme", {4, 1, 2}, 5),
        tip("P3", "Aiguille", {1, 1, 5}, 290),
        tip("P4", "Bille", {2, 3, 2}, 160),
        tip("P5", "Plateau", {3, 2, 2}, 40),
        tip("P6", "Pointe Fine", {1, 2, 4}, 260),
        tip("P7", "Crampon", {5, 1, 1}, 20),
        tip("P8", "Dôme", {1, 5, 1}, 210),
        tip("P9", "Rotule", {2, 2, 3}, 180),
        tip("P10", "Sabot", {3, 3, 1}, 60),

        // Boulons (aucune statistique) : chacun débloque un coup spécial.
        bolt("B1", "Boulon Tempête", "onde", 200),
        bolt("B2", "Boulon Perçant", "lance", 0),
        bolt("B3", "Boulon Polaire", "givre", 190),
        bolt("B4", "Boulon Abysse", "vortex", 270),
        bolt("B5", "Boulon Spectral", "distorsion", 315),
        bolt("B6", "Boulon Vampire", "siphon", 350),
        bolt("B7", "Boulon Tellurique", "seisme", 30),
    };
}

} // namespace

const PartCatalog &PartCatalog::instance()
{
    static const PartCatalog catalog(defaultParts());
    return catalog;
}

PartCatalog::PartCatalog(QList<Part> parts)
    : m_parts(std::move(parts))
{
}

QList<Part> PartCatalog::parts(PartKind kind) const
{
    QList<Part> result;
    for (const Part &part : m_parts) {
        if (part.kind == kind)
            result.append(part);
    }
    return result;
}

std::optional<Part> PartCatalog::find(const QString &id) const
{
    for (const Part &part : m_parts) {
        if (part.id == id)
            return part;
    }
    return std::nullopt;
}

QList<Top> PartCatalog::presets() const
{
    struct Recipe
    {
        const char *name;
        const char *wheel;
        const char *middle;
        const char *tip;
        const char *bolt;
    };
    static constexpr Recipe recipes[] = {
        {"Cyclone Furieux", "RM2", "PI2", "P2", "B2"},   // 14 / 4 / 5   -> Attaque
        {"Forteresse", "RM3", "PI3", "P1", "B1"},        //  4 / 14 / 5  -> Défense
        {"Éternelle", "RM4", "PI4", "P3", "B6"},         //  4 / 4 / 15  -> Endurance
        {"Juste Milieu", "RM5", "PI5", "P4", "B3"},      //  8 / 8 / 7   -> Équilibre
        {"Gardienne", "RM1", "PI1", "P1", "B7"},         //  6 / 12 / 5  -> Défense
        {"Comète", "RM9", "PI7", "P7", "B5"},            // 15 / 5 / 3   -> Attaque
        {"Colosse", "RM10", "PI8", "P8", "B4"},          //  3 / 15 / 5  -> Défense
        {"Brise Éternelle", "RM13", "PI9", "P9", "B3"},  //  7 / 5 / 11  -> Endurance
    };

    QList<Top> result;
    for (const Recipe &recipe : recipes) {
        const auto w = find(QString::fromLatin1(recipe.wheel));
        const auto m = find(QString::fromLatin1(recipe.middle));
        const auto t = find(QString::fromLatin1(recipe.tip));
        const auto b = find(QString::fromLatin1(recipe.bolt));
        if (w && m && t && b)
            result.append(Top(QString::fromUtf8(recipe.name), *w, *m, *t, *b));
    }
    return result;
}

int PartCatalog::maxStatValue() const
{
    int best = 0;
    auto bestOf = [this](PartKind kind, int Stats::*field) {
        int value = 0;
        for (const Part &part : parts(kind))
            value = std::max(value, part.stats.*field);
        return value;
    };
    for (int Stats::*field : {&Stats::attack, &Stats::defense, &Stats::endurance}) {
        best = std::max(best, bestOf(PartKind::Wheel, field) + bestOf(PartKind::Middle, field)
                                  + bestOf(PartKind::Tip, field));
    }
    return best;
}

int PartCatalog::budget(PartKind kind)
{
    switch (kind) {
    case PartKind::Wheel:
        return kWheelBudget;
    case PartKind::Middle:
        return kMiddleBudget;
    case PartKind::Bolt:
        return 0;
    case PartKind::Tip:
        break;
    }
    return kTipBudget;
}

} // namespace toupie
