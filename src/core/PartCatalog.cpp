#include "core/PartCatalog.h"

#include <utility>

namespace toupie {

namespace {

Part wheel(const char *id, const char *name, Stats stats, SpinDirection spin, int hue)
{
    return {QString::fromLatin1(id), QString::fromUtf8(name), PartKind::Wheel, stats, spin, hue};
}

Part middle(const char *id, const char *name, Stats stats, int hue)
{
    return {QString::fromLatin1(id), QString::fromUtf8(name), PartKind::Middle, stats,
            SpinDirection::Clockwise, hue};
}

Part tip(const char *id, const char *name, Stats stats, int hue)
{
    return {QString::fromLatin1(id), QString::fromUtf8(name), PartKind::Tip, stats,
            SpinDirection::Clockwise, hue};
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

        // Parties intermédiaires (budget 6).
        middle("PI1", "Noyau", {2, 3, 1}, 200),
        middle("PI2", "Griffe", {4, 1, 1}, 10),
        middle("PI3", "Carapace", {1, 4, 1}, 130),
        middle("PI4", "Anneau", {1, 1, 4}, 280),
        middle("PI5", "Axe", {2, 2, 2}, 50),
        middle("PI6", "Turbine", {3, 1, 2}, 30),

        // Pointes (budget 7).
        tip("P1", "Pivot", {1, 4, 2}, 220),
        tip("P2", "Gomme", {4, 1, 2}, 5),
        tip("P3", "Aiguille", {1, 1, 5}, 290),
        tip("P4", "Bille", {2, 3, 2}, 160),
        tip("P5", "Plateau", {3, 2, 2}, 40),
        tip("P6", "Pointe Fine", {1, 2, 4}, 260),
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
    };
    static constexpr Recipe recipes[] = {
        {"Cyclone Furieux", "RM2", "PI2", "P2"},   // 14 / 4 / 5   -> Attaque
        {"Forteresse", "RM3", "PI3", "P1"},        //  4 / 14 / 5  -> Défense
        {"Éternelle", "RM4", "PI4", "P3"},         //  4 / 4 / 15  -> Endurance
        {"Juste Milieu", "RM5", "PI5", "P4"},      //  8 / 8 / 7   -> Équilibre
        {"Gardienne", "RM1", "PI1", "P1"},         //  6 / 12 / 5  -> Défense
    };

    QList<Top> result;
    for (const Recipe &recipe : recipes) {
        const auto w = find(QString::fromLatin1(recipe.wheel));
        const auto m = find(QString::fromLatin1(recipe.middle));
        const auto t = find(QString::fromLatin1(recipe.tip));
        if (w && m && t)
            result.append(Top(QString::fromUtf8(recipe.name), *w, *m, *t));
    }
    return result;
}

int PartCatalog::budget(PartKind kind)
{
    switch (kind) {
    case PartKind::Wheel:
        return kWheelBudget;
    case PartKind::Middle:
        return kMiddleBudget;
    case PartKind::Tip:
        break;
    }
    return kTipBudget;
}

} // namespace toupie
