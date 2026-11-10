#include "ui/TopPainter.h"

#include "core/Top.h"
#include "game/SpecialMove.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTransform>
#include <QtMath>

#include <cmath>
#include <functional>

namespace toupie::TopPainter {

namespace {

constexpr double kTau = 2.0 * M_PI;

double frac(double x)
{
    return x - std::floor(x);
}

double smooth(double a, double b, double x)
{
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

using Profile = std::function<double(double)>;

/// Silhouette de chaque roue métallique : rayon relatif r(θ) ∈ [0, 1].
Profile wheelProfile(const QString &id)
{
    static const QHash<QString, Profile> profiles{
        {QStringLiteral("RM1"), [](double t) { // Rempart : six boucliers
             const double u = frac(t * 6 / kTau);
             return 0.8 + 0.2 * smooth(0.06, 0.16, u) * (1 - smooth(0.84, 0.94, u));
         }},
        {QStringLiteral("RM2"), [](double t) { // Lame Cyclone : trois crochets
             const double u = frac(t * 3 / kTau);
             return 0.72 + 0.28 * std::pow(u, 2.4) * (1 - smooth(0.96, 1, u));
         }},
        {QStringLiteral("RM3"), [](double t) { // Bastion : huit bastions
             const double u = frac(t * 8 / kTau);
             return 0.86 + 0.14 * smooth(0.12, 0.25, u) * (1 - smooth(0.75, 0.88, u));
         }},
        {QStringLiteral("RM4"), [](double t) { // Éclipse : disque échancré
             return 0.97 - 0.22 * std::pow(std::max(0.0, std::cos(t - 0.6)), 60)
                  - 0.1 * std::pow(std::max(0.0, std::cos(t + 2.5)), 60);
         }},
        {QStringLiteral("RM5"), [](double t) { // Prisme : hexagone cristallin
             const double a = frac(t * 6 / kTau) * kTau / 6 - M_PI / 6;
             return std::cos(M_PI / 6) / std::cos(a) * 0.95;
         }},
        {QStringLiteral("RM6"), [](double t) { // Tonnerre : cinq éclairs
             const double u = frac(t * 5 / kTau);
             const double z = u < 0.5 ? u * 2 : (u < 0.62 ? 0.55 : 0.55 + (u - 0.62) * 1.18);
             return 0.7 + 0.3 * z * (1 - smooth(0.95, 1, u));
         }},
        {QStringLiteral("RM7"), [](double t) { // Ouragan : quatre ailes
             const double u = frac(t * 4 / kTau);
             return 0.72 + 0.28 * std::pow(std::sin(M_PI * std::pow(u, 0.6)), 1.5);
         }},
        {QStringLiteral("RM8"), [](double t) { // Orbite : deux masses
             return 0.84 + 0.16 * std::pow(std::max(0.0, std::cos(2 * t)), 8);
         }},
        {QStringLiteral("RM9"), [](double t) { // Météore : deux éperons
             const double u = frac(t * 2 / kTau);
             return 0.7 + 0.3 * std::pow(u, 3.5) * (1 - smooth(0.97, 1, u));
         }},
        {QStringLiteral("RM10"), [](double t) { // Titan : quatre butoirs
             const double u = frac(t * 4 / kTau);
             return 0.82 + 0.18 * smooth(0.1, 0.18, u) * (1 - smooth(0.62, 0.7, u));
         }},
        {QStringLiteral("RM11"), [](double t) { // Nébuleuse : bord ondulé
             return 0.95 + 0.035 * std::sin(t * 14) + 0.015 * std::sin(t * 5 + 1);
         }},
        {QStringLiteral("RM12"), [](double t) { // Faucon : trois serres
             const double u = frac(t * 3 / kTau);
             return 0.7 + 0.3 * std::pow(std::max(0.0, std::sin(M_PI * std::pow(u, 2.2))), 3);
         }},
        {QStringLiteral("RM13"), [](double t) { // Zéphyr : six ailettes
             const double u = frac(t * 6 / kTau);
             return 0.8 + 0.2 * std::pow(u, 1.2) * (1 - smooth(0.9, 1, u));
         }},
        {QStringLiteral("RM14"), [](double t) { // Golem : sept rocs
             const double u = frac(t * 7 / kTau);
             const double i = std::floor(t * 7 / kTau);
             return 0.84 + (0.1 + 0.06 * std::sin(i * 2.7)) * smooth(0.05, 0.2, u) * (1 - smooth(0.7, 0.92, u));
         }},
    };
    const auto it = profiles.constFind(id);
    if (it != profiles.constEnd())
        return *it;
    return [](double t) { return 0.86 + 0.14 * std::pow(std::max(0.0, std::cos(3 * t)), 4); };
}

/// Contour d'une roue de rayon 1 (mis en cache) ; la roue antihoraire est en miroir.
const QPainterPath &unitWheel(const QString &id, bool mirror, double scale)
{
    static QHash<QString, QPainterPath> cache;
    const QString key = QStringLiteral("%1/%2/%3").arg(id).arg(mirror).arg(scale);
    auto it = cache.find(key);
    if (it == cache.end()) {
        const Profile r = wheelProfile(id);
        QPolygonF poly;
        constexpr int kSamples = 200;
        for (int i = 0; i < kSamples; ++i) {
            const double t = i * kTau / kSamples;
            const double rr = r(mirror ? kTau - t : t) * scale;
            poly << QPointF(std::cos(t) * rr, std::sin(t) * rr);
        }
        QPainterPath path;
        path.addPolygon(poly);
        path.closeSubpath();
        it = cache.insert(key, path);
    }
    return *it;
}

QColor hsl(int hue, double s, double l, double a = 1.0)
{
    QColor c = QColor::fromHslF(((hue % 360) + 360) % 360 / 360.0, s, l);
    c.setAlphaF(a);
    return c;
}

QPainterPath glyphPath(const QString &special)
{
    QPainterPath p;
    if (special == QLatin1String("onde")) {
        p.addEllipse(QPointF(0, 0), 0.25, 0.25);
        const QRectF ring(-0.65, -0.65, 1.3, 1.3);
        for (int k = 0; k < 4; ++k) {
            p.arcMoveTo(ring, 45 + 90 * k - 28);
            p.arcTo(ring, 45 + 90 * k - 28, 56);
        }
    } else if (special == QLatin1String("lance")) {
        p.addPolygon(QPolygonF{{0, -0.95}, {0.3, -0.38}, {0.11, -0.42}, {0.11, 0.62}, {0.26, 0.9},
                               {-0.26, 0.9}, {-0.11, 0.62}, {-0.11, -0.42}, {-0.3, -0.38}, {0, -0.95}});
    } else if (special == QLatin1String("givre")) {
        for (int k = 0; k < 3; ++k) {
            const double a = k * M_PI / 3 - M_PI / 2;
            const QPointF d(std::cos(a), std::sin(a));
            const QPointF n(-d.y(), d.x());
            p.moveTo(-d * 0.9);
            p.lineTo(d * 0.9);
            for (double s : {1.0, -1.0}) {
                p.moveTo(d * s * 0.75 + n * 0.22);
                p.lineTo(d * s * 0.52);
                p.lineTo(d * s * 0.75 - n * 0.22);
            }
        }
    } else if (special == QLatin1String("vortex")) {
        p.moveTo(0, 0);
        p.cubicTo(0.15, -0.05, 0.2, 0.18, 0.05, 0.25);
        p.cubicTo(-0.2, 0.35, -0.4, 0.05, -0.3, -0.2);
        p.cubicTo(-0.15, -0.55, 0.35, -0.55, 0.52, -0.15);
        p.cubicTo(0.7, 0.3, 0.35, 0.75, -0.1, 0.75);
        p.cubicTo(-0.6, 0.75, -0.92, 0.3, -0.85, -0.2);
    } else if (special == QLatin1String("distorsion")) {
        p.moveTo(-0.9, 0);
        p.cubicTo(-0.5, -0.55, 0.5, -0.55, 0.9, 0);
        p.cubicTo(0.5, 0.55, -0.5, 0.55, -0.9, 0);
        p.addEllipse(QPointF(0, 0), 0.22, 0.22);
        p.moveTo(-0.6, -0.72);
        p.lineTo(-0.05, -0.72);
        p.moveTo(0.1, -0.72);
        p.lineTo(0.35, -0.72);
        p.moveTo(-0.3, 0.72);
        p.lineTo(0.6, 0.72);
    } else if (special == QLatin1String("siphon")) {
        p.moveTo(0, -0.9);
        p.cubicTo(0.35, -0.35, 0.6, 0, 0.6, 0.3);
        p.arcTo(QRectF(-0.6, -0.3, 1.2, 1.2), 0, -180);
        p.cubicTo(-0.6, 0, -0.35, -0.35, 0, -0.9);
        p.moveTo(-0.25, 0.1);
        p.lineTo(-0.12, 0.45);
        p.lineTo(0, 0.1);
        p.lineTo(0.12, 0.45);
        p.lineTo(0.25, 0.1);
    } else if (special == QLatin1String("seisme")) {
        p.addPolygon(QPolygonF{{-0.95, 0.55}, {-0.45, -0.25}, {-0.15, 0.15}, {0.2, -0.6}, {0.95, 0.55}, {-0.95, 0.55}});
        p.moveTo(-0.55, 0.55);
        p.lineTo(-0.25, 0.2);
        p.lineTo(0.05, 0.4);
        p.lineTo(0.2, 0.05);
    } else {
        p.addEllipse(QPointF(0, 0), 0.4, 0.4);
    }
    return p;
}

void paintMiddle(QPainter &p, const Part &middle, double R, double rot)
{
    const QColor col = hsl(middle.hue, 0.95, 0.62);
    QRadialGradient disc(QPointF(0, 0), R);
    disc.setColorAt(0.0, QColor(0x2a, 0x24, 0x70));
    disc.setColorAt(0.7, QColor(0x14, 0x10, 0x38));
    disc.setColorAt(1.0, QColor(0x07, 0x06, 0x1a));
    p.setBrush(disc);
    p.setPen(QPen(Qt::black, std::max(0.6, R * 0.02)));
    p.drawEllipse(QPointF(0, 0), R, R);

    auto at = [&](double a, double r) { return QPointF(std::cos(a + rot), std::sin(a + rot)) * r; };
    const double line = std::max(0.8, R * 0.045);
    const QString &id = middle.id;
    p.setPen(Qt::NoPen);
    if (id == QLatin1String("PI2")) { // Griffe
        p.setBrush(col);
        for (int i = 0; i < 3; ++i) {
            const double a = i * kTau / 3;
            p.drawPolygon(QPolygonF{at(a - 0.25, R * 0.55), at(a + 0.1, R * 1.0), at(a + 0.32, R * 0.6)});
        }
    } else if (id == QLatin1String("PI3")) { // Carapace
        p.setPen(QPen(col, line * 0.8));
        p.setBrush(QColor(0x1b, 0x16, 0x50));
        for (int i = 0; i < 6; ++i) {
            const QPointF c = at(i * kTau / 6, R * 0.7);
            QPolygonF hexagon;
            for (int k = 0; k < 6; ++k)
                hexagon << c + QPointF(std::cos(k * kTau / 6), std::sin(k * kTau / 6)) * R * 0.21;
            p.drawPolygon(hexagon);
        }
    } else if (id == QLatin1String("PI4")) { // Anneau
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i) {
            QColor c = col;
            c.setAlphaF(1.0 - i * 0.25);
            p.setPen(QPen(c, i == 0 ? line : line * 0.5));
            p.drawEllipse(QPointF(0, 0), R * (0.95 - i * 0.15), R * (0.95 - i * 0.15));
        }
    } else if (id == QLatin1String("PI5")) { // Axe
        p.setBrush(col);
        for (int i = 0; i < 4; ++i) {
            const double a = i * kTau / 4;
            p.drawPolygon(QPolygonF{at(a - 0.14, R * 0.5), at(a - 0.08, R * 0.97), at(a + 0.08, R * 0.97), at(a + 0.14, R * 0.5)});
        }
    } else if (id == QLatin1String("PI6")) { // Turbine
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(col, line, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 9; ++i) {
            const double a = i * kTau / 9;
            QPainterPath blade(at(a, R * 0.5));
            blade.quadTo(at(a + 0.1, R * 0.95), at(a + 0.55, R * 0.97));
            p.drawPath(blade);
        }
    } else if (id == QLatin1String("PI7")) { // Lame
        p.setBrush(col);
        for (int i = 0; i < 2; ++i) {
            const double a = i * M_PI;
            p.drawPolygon(QPolygonF{at(a - 0.5, R * 0.5), at(a - 0.05, R * 1.02), at(a + 0.6, R * 0.62)});
        }
    } else if (id == QLatin1String("PI8")) { // Bouclier
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(col, R * 0.16, Qt::SolidLine, Qt::FlatCap));
        const QRectF arc(-R * 0.86, -R * 0.86, R * 1.72, R * 1.72);
        for (int i = 0; i < 4; ++i)
            p.drawArc(arc, int((-qRadiansToDegrees(rot) + i * 90 - 34) * 16), 68 * 16);
    } else if (id == QLatin1String("PI9")) { // Spirale
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(col, line * 1.1, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 3; ++i) {
            QPainterPath arm(at(i * kTau / 3, R * 0.45));
            for (int k = 1; k <= 20; ++k)
                arm.lineTo(at(i * kTau / 3 + k * 0.12, R * (0.45 + k * 0.026)));
            p.drawPath(arm);
        }
    } else if (id == QLatin1String("PI10")) { // Contrepoids
        p.setBrush(col);
        for (int i = 0; i < 2; ++i) {
            const double a = i * M_PI + M_PI / 2;
            p.save();
            p.translate(at(a, R * 0.7));
            p.rotate(qRadiansToDegrees(a + rot) + 90);
            p.drawEllipse(QPointF(0, 0), R * 0.28, R * 0.17);
            p.restore();
        }
    } else { // Noyau (et pièce inconnue)
        for (int i = 0; i < 4; ++i) {
            const QPointF c = at(i * kTau / 4 + 0.78, R * 0.72);
            p.setPen(QPen(col, line * 0.7));
            p.setBrush(QColor(0x1b, 0x16, 0x50));
            p.drawEllipse(c, R * 0.13, R * 0.13);
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            p.drawEllipse(c, R * 0.045, R * 0.045);
        }
    }
    QColor rim = col;
    rim.setAlphaF(0.45);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rim, std::max(0.6, R * 0.02)));
    p.drawEllipse(QPointF(0, 0), R, R);
}

void paintBolt(QPainter &p, const Top &top, double R, double rot)
{
    const QColor col = moveColor(top.special());
    QPolygonF octagon;
    for (int i = 0; i < 8; ++i) {
        const double a = i * kTau / 8 + M_PI / 8 + rot;
        octagon << QPointF(std::cos(a), std::sin(a)) * R;
    }
    QLinearGradient metal(QPointF(-R, -R), QPointF(R, R));
    metal.setColorAt(0.0, QColor(0xe9, 0xee, 0xff));
    metal.setColorAt(0.5, QColor(0x7e, 0x86, 0xbe));
    metal.setColorAt(1.0, QColor(0x26, 0x2a, 0x5a));
    p.setBrush(metal);
    p.setPen(QPen(QColor(0x05, 0x04, 0x0f), std::max(0.6, R * 0.04)));
    p.drawPolygon(octagon);

    QRadialGradient core(QPointF(-R * 0.12, -R * 0.18), R * 0.85);
    core.setColorAt(0.0, col);
    core.setColorAt(0.6, QColor(0x1a, 0x16, 0x50));
    core.setColorAt(1.0, QColor(0x07, 0x06, 0x1a));
    p.setBrush(core);
    p.setPen(QPen(col, std::max(0.6, R * 0.05)));
    p.drawEllipse(QPointF(0, 0), R * 0.8, R * 0.8);
    if (R >= 6.0)
        paintEmblem(p, top.special(), QPointF(0, 0), R * 0.5, Qt::white, std::max(0.8, R * 0.08));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 90));
    p.save();
    p.translate(-R * 0.28, -R * 0.36);
    p.rotate(-35);
    p.drawEllipse(QPointF(0, 0), R * 0.26, R * 0.12);
    p.restore();
}

} // namespace

QColor typeColor(TopType type)
{
    switch (type) {
    case TopType::Attack:
        return QColor(0xff, 0x3d, 0x6e);
    case TopType::Defense:
        return QColor(0x2f, 0x7b, 0xff);
    case TopType::Endurance:
        return QColor(0x3d, 0xff, 0x8b);
    case TopType::Balance:
        return QColor(0xff, 0xc2, 0x3d);
    }
    return Qt::gray;
}

QColor moveColor(const QString &special)
{
    const auto move = SpecialMove::find(special);
    return move ? QColor::fromRgb(move->color) : QColor(Qt::white);
}

void paintEmblem(QPainter &painter, const QString &special, QPointF center, double size,
                 const QColor &color, double penWidth)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const QPainterPath path
        = QTransform::fromTranslate(center.x(), center.y()).scale(size, size).map(glyphPath(special));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
    painter.restore();
}

void paint(QPainter &painter, const Top &top, QPointF center, double radius, double angle,
           double glow)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(center);

    const QColor type = typeColor(top.type());
    const Part &wheel = top.wheel();
    const bool mirror = wheel.spin == SpinDirection::CounterClockwise;
    const QColor accent = hsl(wheel.hue, 0.95, 0.6);
    const QColor deep = hsl(wheel.hue, 0.8, 0.22);

    // Ombre portée.
    QRadialGradient shadow(QPointF(radius * 0.1, radius * 0.16), radius * 1.1);
    shadow.setColorAt(0.75, QColor(0, 0, 0, 120));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadow);
    painter.drawEllipse(QPointF(radius * 0.1, radius * 0.16), radius * 1.1, radius * 1.1);

    if (glow > 0.0) {
        QRadialGradient halo(QPointF(0, 0), radius * 1.9);
        QColor c = type;
        c.setAlphaF(0.6 * glow);
        halo.setColorAt(0.45, c);
        c.setAlphaF(0.0);
        halo.setColorAt(1.0, c);
        painter.setBrush(halo);
        painter.drawEllipse(QPointF(0, 0), radius * 1.9, radius * 1.9);
    }

    // Roue métallique : la silhouette tourne, la lumière reste fixe.
    const QTransform spin = QTransform().rotateRadians(angle).scale(radius, radius);
    const QPainterPath silhouette = spin.map(unitWheel(wheel.id, mirror, 1.0));
    QRadialGradient metal(QPointF(-radius * 0.35, -radius * 0.4), radius * 1.6);
    metal.setColorAt(0.0, QColor(0xf4, 0xf7, 0xff));
    metal.setColorAt(0.45, QColor(0x9a, 0xa4, 0xd8));
    metal.setColorAt(0.8, QColor(0x4b, 0x52, 0x90));
    metal.setColorAt(1.0, QColor(0x1d, 0x21, 0x50));
    painter.setBrush(metal);
    painter.setPen(QPen(QColor(0x05, 0x04, 0x0f), std::max(0.8, radius * 0.02)));
    painter.drawPath(silhouette);
    QLinearGradient tint(QPointF(-radius, -radius), QPointF(radius, radius));
    QColor a1 = accent, a2 = deep;
    a1.setAlphaF(0.85);
    a2.setAlphaF(0.6);
    tint.setColorAt(0.0, a1);
    tint.setColorAt(0.5, a2);
    a1.setAlphaF(0.9);
    tint.setColorAt(1.0, a1);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tint);
    painter.drawPath(silhouette);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 115), std::max(0.6, radius * 0.014)));
    painter.drawPath(spin.map(unitWheel(wheel.id, mirror, 0.86)));

    if (radius >= 10.0) {
        // Rainures radiales.
        QPainterPath grooves;
        for (int i = 0; i < 24; ++i) {
            const double a = i * kTau / 24 + angle;
            const QPointF d(std::cos(a), std::sin(a));
            grooves.moveTo(d * radius * 0.66);
            grooves.lineTo(d * radius * 0.76);
        }
        painter.setPen(QPen(QColor(5, 4, 15, 140), std::max(0.7, radius * 0.013)));
        painter.drawPath(grooves);
    }
    painter.setPen(QPen(accent, std::max(0.8, radius * 0.022)));
    painter.drawEllipse(QPointF(0, 0), radius * 0.79, radius * 0.79);
    // Reflet métallique fixe (haut-gauche).
    painter.setPen(QPen(QColor(255, 255, 255, 130), std::max(1.0, radius * 0.03), Qt::SolidLine,
                        Qt::RoundCap));
    painter.drawArc(QRectF(-radius * 0.87, -radius * 0.87, radius * 1.74, radius * 1.74), 100 * 16,
                    70 * 16);

    paintMiddle(painter, top.middle(), radius * 0.6, angle);
    if (top.bolt().isValid())
        paintBolt(painter, top, radius * 0.34, angle);
    painter.restore();
}

void paintSpinArrow(QPainter &painter, SpinDirection direction, QPointF center, double radius,
                    const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, qMax(1.5, radius * 0.12), Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);

    const QRectF rect(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius);
    // Arc de 270° ; Qt compte les angles dans le sens anti-horaire.
    painter.drawArc(rect, 90 * 16, -270 * 16);

    // Pointe de flèche au bout de l'arc (horaire : fin de l'arc à 180°).
    const bool clockwise = direction == SpinDirection::Clockwise;
    const double endDeg = clockwise ? 180.0 : 90.0;
    const QPointF end(center.x() + radius * qCos(qDegreesToRadians(endDeg)),
                      center.y() - radius * qSin(qDegreesToRadians(endDeg)));
    // Direction tangente au point final, dans le sens de rotation affiché.
    const QPointF tangent = clockwise ? QPointF(0, -1) : QPointF(-1, 0);
    const QPointF normal(-tangent.y(), tangent.x());
    const double s = radius * 0.5;
    QPolygonF head;
    head << end + tangent * s * 0.6 << end - normal * s * 0.5 << end + normal * s * 0.5;
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(head);
    painter.restore();
}

} // namespace toupie::TopPainter
