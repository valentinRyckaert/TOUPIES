#include "game/Stadium.h"

#include <algorithm>
#include <cmath>

namespace toupie {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

// ---------------------------------------------------------------------------
// Géométrie
// ---------------------------------------------------------------------------

double StadiumGeometry::apothem() const
{
    return radius * std::cos(kPi / sides);
}

Vec2 StadiumGeometry::sideNormal(int index) const
{
    return Vec2::fromAngle(index * 2.0 * kPi / sides);
}

double StadiumGeometry::level(Vec2 p, double inset) const
{
    switch (kind) {
    case Kind::Circle:
        return p.length() / std::max(1e-9, radius - inset);
    case Kind::Polygon: {
        double best = 0.0;
        for (int i = 0; i < sides; ++i)
            best = std::max(best, p.dot(sideNormal(i)));
        return best / std::max(1e-9, apothem() - inset);
    }
    case Kind::Ellipse: {
        const double a = std::max(1e-9, radius - inset);
        const double b = std::max(1e-9, radius * aspect - inset);
        return std::hypot(p.x / a, p.y / b);
    }
    }
    return 0.0;
}

Vec2 StadiumGeometry::outwardNormal(Vec2 p) const
{
    switch (kind) {
    case Kind::Circle:
        break;
    case Kind::Polygon: {
        int bestIndex = 0;
        double best = -1e18;
        for (int i = 0; i < sides; ++i) {
            const double d = p.dot(sideNormal(i));
            if (d > best) {
                best = d;
                bestIndex = i;
            }
        }
        return sideNormal(bestIndex);
    }
    case Kind::Ellipse: {
        const double a = radius;
        const double b = radius * aspect;
        const Vec2 n = Vec2{p.x / (a * a), p.y / (b * b)}.normalized();
        return n.lengthSquared() > 0.0 ? n : Vec2{1.0, 0.0};
    }
    }
    const Vec2 n = p.normalized();
    return n.lengthSquared() > 0.0 ? n : Vec2{1.0, 0.0};
}

Vec2 StadiumGeometry::projectInside(Vec2 p, double inset) const
{
    const double l = level(p, inset);
    if (l <= 1.0)
        return p;
    if (kind == Kind::Polygon) {
        // Projection successive sur chaque côté dépassé (coins compris).
        Vec2 q = p;
        for (int i = 0; i < sides; ++i) {
            const Vec2 n = sideNormal(i);
            const double excess = q.dot(n) - (apothem() - inset);
            if (excess > 0.0)
                q -= n * excess;
        }
        return q;
    }
    return p / l;
}

double StadiumGeometry::innerRadius() const
{
    switch (kind) {
    case Kind::Circle:
        return radius;
    case Kind::Polygon:
        return apothem();
    case Kind::Ellipse:
        return radius * std::min(1.0, aspect);
    }
    return radius;
}

QList<Vec2> StadiumGeometry::outline(int segments) const
{
    QList<Vec2> points;
    if (kind == Kind::Polygon) {
        for (int i = 0; i < sides; ++i)
            points.append(Vec2::fromAngle((i + 0.5) * 2.0 * kPi / sides) * radius);
        return points;
    }
    const double b = kind == Kind::Ellipse ? radius * aspect : radius;
    for (int i = 0; i < segments; ++i) {
        const double t = i * 2.0 * kPi / segments;
        points.append({radius * std::cos(t), b * std::sin(t)});
    }
    return points;
}

// ---------------------------------------------------------------------------
// Types de stadium
// ---------------------------------------------------------------------------

BattleConfig Stadium::apply(const BattleConfig &base) const
{
    BattleConfig c = base;
    c.stadiumRadius *= size;
    c.rimEscapeSpeed *= rimHeight;
    c.slopeStrength *= slope;
    c.friction *= friction;
    c.orbitBase *= speed;
    c.orbitPerAttack *= speed;
    c.passiveBase *= enduranceDrain;
    c.damageBase *= damage;
    c.knockbackBase *= knockback;
    c.knockbackPerAttack *= knockback;
    c.knockbackPerAttackerDefense *= knockback;
    c.knockbackApproach *= knockback;
    c.dashSpeed *= dashSpeed;
    c.dashAttackMultiplier = 1.0 + (base.dashAttackMultiplier - 1.0) * dashPower;
    return c;
}

StadiumGeometry Stadium::geometry(const BattleConfig &applied) const
{
    StadiumGeometry g;
    g.kind = shape;
    g.radius = applied.stadiumRadius;
    g.sides = sides;
    g.aspect = aspect;
    return g;
}

const QList<Stadium> &Stadium::all()
{
    static const QList<Stadium> stadiums = [] {
        using Kind = StadiumGeometry::Kind;
        QList<Stadium> list;

        Stadium classic;
        classic.id = QStringLiteral("classique");
        classic.name = QStringLiteral("Classique");
        classic.description = QStringLiteral(
            "Cuvette circulaire en plastique avec rebord. Le stadium de référence, sans "
            "avantage pour aucun type.");
        list.append(classic);

        Stadium concrete;
        concrete.id = QStringLiteral("beton");
        concrete.name = QStringLiteral("Béton");
        concrete.description = QStringLiteral(
            "Octogone au sol dur et lisse, entouré d'un très haut rebord. Les toupies vont "
            "un peu plus vite et les chocs sont plus violents, mais la surface râpe la "
            "pointe.");
        concrete.shape = Kind::Polygon;
        concrete.sides = 8;
        concrete.rimHeight = 2.0;
        concrete.friction = 0.85;
        concrete.speed = 1.05;
        concrete.damage = 1.1;
        concrete.knockback = 0.9;
        concrete.enduranceDrain = 1.1;
        concrete.floorCenter = 0xc4c6c9;
        concrete.floorEdge = 0x73777c;
        concrete.rimColor = 0x3d4045;
        list.append(concrete);

        Stadium dirt;
        dirt.id = QStringLiteral("terre");
        dirt.name = QStringLiteral("Terre");
        dirt.description = QStringLiteral(
            "Arène de terre battue sans rebord. Le sol accroche la pointe : la rotation "
            "s'use plus vite et les chocs frappent fort ; les dashs sont un peu plus "
            "courts.");
        dirt.hasRim = false;
        dirt.friction = 1.0;
        dirt.speed = 1.0;
        dirt.dashSpeed = 0.95;
        dirt.damage = 1.4;
        dirt.knockback = 1.0;
        dirt.enduranceDrain = 1.2;
        dirt.floorCenter = 0xc39663;
        dirt.floorEdge = 0x6e4b2a;
        dirt.rimColor = 0x2b1b0e;
        list.append(dirt);

        Stadium ice;
        ice.id = QStringLiteral("glace");
        ice.name = QStringLiteral("Glace");
        ice.description = QStringLiteral(
            "Patinoire elliptique au haut rebord. La glace use très peu la rotation et les "
            "chocs repoussent moins : idéal pour durer.");
        ice.shape = Kind::Ellipse;
        ice.aspect = 0.8;
        ice.rimHeight = 1.7;
        ice.friction = 0.9;
        ice.slope = 1.0;
        ice.enduranceDrain = 0.7;
        ice.knockback = 0.85;
        ice.damage = 0.9;
        ice.floorCenter = 0xf1f8ff;
        ice.floorEdge = 0x92badb;
        ice.rimColor = 0x4d6f8c;
        list.append(ice);

        Stadium sand;
        sand.id = QStringLiteral("sable");
        sand.name = QStringLiteral("Sable");
        sand.description = QStringLiteral(
            "Hexagone de sable aux rebords très hauts. Le sable ralentit tout et use vite la "
            "rotation, mais un dash bien placé frappe très fort.");
        sand.shape = Kind::Polygon;
        sand.sides = 6;
        sand.rimHeight = 1.5;
        sand.friction = 1.3;
        sand.speed = 0.9;
        sand.enduranceDrain = 1.3;
        sand.damage = 1.3;
        sand.dashSpeed = 0.9;
        sand.dashPower = 2.0;
        sand.floorCenter = 0xf2dca4;
        sand.floorEdge = 0xb89455;
        sand.rimColor = 0x7a5c2c;
        list.append(sand);

        Stadium volcano;
        volcano.id = QStringLiteral("volcan");
        volcano.name = QStringLiteral("Volcan");
        volcano.description = QStringLiteral(
            "Petit cratère aux pentes très raides, sans rebord, entouré de lave. Les chocs "
            "et les dashs y sont plus puissants.");
        volcano.size = 0.9;
        volcano.hasRim = false;
        volcano.slope = 2.0;
        volcano.damage = 1.1;
        volcano.dashPower = 1.2;
        volcano.enduranceDrain = 1.0;
        volcano.floorCenter = 0x6a4741;
        volcano.floorEdge = 0x2a1512;
        volcano.rimColor = 0xff5a1f;
        list.append(volcano);

        return list;
    }();
    return stadiums;
}

const Stadium &Stadium::classic()
{
    return all().first();
}

std::optional<Stadium> Stadium::find(const QString &id)
{
    for (const Stadium &stadium : all()) {
        if (stadium.id == id)
            return stadium;
    }
    return std::nullopt;
}

} // namespace toupie
