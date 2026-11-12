#include "ui/StadiumPainter.h"

#include "ui/Theme.h"
#include "ui/TopPainter.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QStringList>
#include <QTextOption>

#include <cmath>
#include <QtMath>

namespace toupie {

namespace {

constexpr double kTau = 2.0 * M_PI;

QPainterPath outlinePath(const StadiumGeometry &geometry, QPointF center, double scale,
                         double grow = 0.0)
{
    QPainterPath path;
    const QList<Vec2> points = geometry.outline();
    for (int i = 0; i < points.size(); ++i) {
        const Vec2 p = points.at(i);
        const double len = p.length();
        const double factor = len > 1e-9 ? (len * scale + grow) / (len * scale) : 1.0;
        const QPointF q = center + QPointF(p.x, p.y) * scale * factor;
        if (i == 0)
            path.moveTo(q);
        else
            path.lineTo(q);
    }
    path.closeSubpath();
    return path;
}

QColor rgb(unsigned value, int alpha = 255)
{
    QColor c = QColor::fromRgb(value);
    c.setAlpha(alpha);
    return c;
}

QPolygonF regular(QPointF c, double r, int n, double rotation)
{
    QPolygonF poly;
    for (int i = 0; i < n; ++i) {
        const double a = rotation + i * kTau / n;
        poly << c + QPointF(std::cos(a), std::sin(a)) * r;
    }
    return poly;
}

/// Faille en zigzag depuis le centre (terre, volcan).
QPainterPath crack(QRandomGenerator &rng, QPointF c, double R, double angle, double from,
                   double step, double wander)
{
    double a = angle, r = R * from;
    QPainterPath path(c + QPointF(std::cos(a), std::sin(a)) * r);
    while (r < R * 0.93) {
        r += R * (step + rng.bounded(step));
        a += (rng.generateDouble() - 0.5) * wander;
        path.lineTo(c + QPointF(std::cos(a), std::sin(a)) * std::min(r, R * 0.95));
    }
    return path;
}

void radialFill(QPainter &p, const QPainterPath &path, QPointF c, double R,
                std::initializer_list<std::pair<double, unsigned>> stops, QPointF focus = {})
{
    QRadialGradient g(c, R, c + focus);
    for (const auto &[at, color] : stops)
        g.setColorAt(at, rgb(color));
    p.fillPath(path, g);
}

/// Projette un point d'un motif plat (dalles, rides) sur la cuvette vue de dessus : le
/// motif suit la pente et se resserre vers le bord, au lieu de paraître posé à plat.
QPointF onBowl(QPointF c, double R, QPointF flat)
{
    const QPointF d = flat - c;
    const double r = std::hypot(d.x(), d.y()) / R;
    if (r < 1e-9)
        return flat;
    constexpr double kReach = 1.25; // Rayon plat ramené sur le bord du stadium.
    const double curved = std::sin(std::min(r, kReach) / kReach * M_PI / 2.0) * 1.04;
    return c + d * (curved / r);
}

/// Ligne plate de a à b, échantillonnée puis projetée sur la cuvette.
void addBowlLine(QPainterPath &path, QPointF c, double R, QPointF a, QPointF b)
{
    constexpr int kSamples = 48;
    for (int i = 0; i <= kSamples; ++i) {
        const QPointF q = onBowl(c, R, a + (b - a) * (double(i) / kSamples));
        i == 0 ? path.moveTo(q) : path.lineTo(q);
    }
}

/// Ombrage d'une cuvette éclairée d'en haut à gauche : le versant opposé (bas-droit)
/// prend la lumière, le versant proche reste dans l'ombre, le fond reste neutre ; le
/// pied du mur s'assombrit.
void bowlShading(QPainter &p, const QPainterPath &floor, QPointF c, double R)
{
    p.save();
    p.setClipPath(floor);
    const QRectF box(c.x() - R * 1.2, c.y() - R * 1.2, R * 2.4, R * 2.4);
    QLinearGradient light(c - QPointF(R, R) * 0.75, c + QPointF(R, R) * 0.75);
    light.setColorAt(0.0, QColor(0, 0, 0, 13));
    light.setColorAt(0.45, QColor(0, 0, 0, 0));
    light.setColorAt(0.55, QColor(255, 255, 255, 0));
    light.setColorAt(1.0, QColor(255, 255, 255, 8));
    // Seulement sur les pentes : fins anneaux superposés, plus marqués vers le bord.
    constexpr int kSteps = 8;
    for (int step = 0; step < kSteps; ++step) {
        const double inner = 0.82 - step * (0.82 - 0.3) / (kSteps - 1);
        QPainterPath ring;
        ring.addEllipse(c, R * 1.3, R * 1.3);
        QPainterPath hole;
        hole.addEllipse(c, R * inner, R * inner);
        p.fillPath(ring.subtracted(hole), light);
    }
    QRadialGradient foot(c, R * 1.05);
    foot.setColorAt(0.62, QColor(0, 0, 0, 0));
    foot.setColorAt(1.0, QColor(0, 0, 0, 150));
    p.fillRect(box, foot);
    p.restore();
}

void rings(QPainter &p, QPointF c, double R, int n, const QColor &color, double aspect = 1.0)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, 1.0));
    for (int i = 1; i <= n; ++i) {
        const double r = R * i / (n + 1);
        p.drawEllipse(c, r, r * aspect);
    }
}

void paintClassic(QPainter &p, const QPainterPath &floor, const StadiumGeometry &g, QPointF c,
                  double scale, double R)
{
    const QPainterPath rim = outlinePath(g, c, scale, R * 0.07);
    Theme::strokeGlow(p, rim, rgb(0x34d6ff, 150), 2.0);
    QLinearGradient rimFill(c - QPointF(R, R), c + QPointF(R, R));
    rimFill.setColorAt(0, rgb(0x3b3490));
    rimFill.setColorAt(1, rgb(0x0f0c33));
    p.fillPath(rim, rimFill);
    // Segments lumineux du rebord.
    const QRectF arc(c.x() - R * 1.035, c.y() - R * 1.035, R * 2.07, R * 2.07);
    for (int i = 0; i < 24; ++i) {
        p.setPen(QPen(i % 3 ? rgb(0x7b3cff, 190) : rgb(0x34d6ff), R * 0.025, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arc, int((i * 15 + 1) * 16), 13 * 16);
    }
    radialFill(p, floor, c, R, {{0.0, 0x2a3c9e}, {0.55, 0x191660}, {1.0, 0x07061a}});
    p.save();
    p.setClipPath(floor);
    rings(p, c, R, 7, rgb(0x34d6ff, 75));
    QPainterPath spokes;
    for (int i = 0; i < 24; ++i) {
        const QPointF d(std::cos(i * kTau / 24), std::sin(i * kTau / 24));
        spokes.moveTo(c + d * R * 0.12);
        spokes.lineTo(c + d * R);
    }
    p.strokePath(spokes, QPen(rgb(0x7b3cff, 64), 1.0));
    QPen dotted(rgb(0x3dff8b, 200), 1.5);
    dotted.setDashPattern({1.5, 7.0});
    p.setPen(dotted);
    p.drawEllipse(c, R * 0.55, R * 0.55);
    p.restore();
    QRadialGradient core(c, R * 0.16);
    core.setColorAt(0, rgb(0x34d6ff, 140));
    core.setColorAt(1, rgb(0x34d6ff, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(core);
    p.drawEllipse(c, R * 0.16, R * 0.16);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rgb(0x34d6ff), 2.0));
    p.drawEllipse(c, R * 0.1, R * 0.1);
    p.setPen(Qt::NoPen);
    p.setBrush(rgb(0x34d6ff, 210));
    p.drawPolygon(regular(c, R * 0.065, 6, 0));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rgb(0x05040f), 3.0));
    p.drawPath(floor);
}

void paintConcrete(QPainter &p, const QPainterPath &floor, const StadiumGeometry &g, QPointF c,
                   double scale, double R, QRandomGenerator &rng)
{
    const QPainterPath wall = outlinePath(g, c, scale, R * 0.16);
    QLinearGradient wallFill(c - QPointF(R, R), c + QPointF(R, R));
    wallFill.setColorAt(0, rgb(0x4a4e66));
    wallFill.setColorAt(0.5, rgb(0x22253a));
    wallFill.setColorAt(1, rgb(0x14162a));
    p.fillPath(wall, wallFill);
    p.setPen(QPen(rgb(0x05040f), 2.0));
    p.drawPath(wall);
    // Bandes de sécurité sur le haut mur.
    QPen stripes(rgb(0x3dff8b, 215), R * 0.1, Qt::CustomDashLine, Qt::FlatCap);
    stripes.setDashPattern({0.35, 0.45});
    p.setPen(stripes);
    p.drawPath(outlinePath(g, c, scale, R * 0.085));
    Theme::strokeGlow(p, outlinePath(g, c, scale, R * 0.02), rgb(0x3dff8b, 110), 1.5);
    radialFill(p, floor, c, R, {{0.0, 0x5d6280}, {0.6, 0x2c3048}, {1.0, 0x121426}});
    p.save();
    p.setClipPath(floor);
    // Dalles de béton qui épousent la cuvette, et poussière.
    QPainterPath seams;
    for (int i = -5; i <= 5; ++i) {
        const double at = i * R * 0.25;
        addBowlLine(seams, c, R, c + QPointF(at, -R * 1.3), c + QPointF(at, R * 1.3));
        addBowlLine(seams, c, R, c + QPointF(-R * 1.3, at), c + QPointF(R * 1.3, at));
    }
    p.strokePath(seams, QPen(QColor(11, 12, 24, 150), 2.0));
    bowlShading(p, floor, c, R);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(154, 160, 192, 60));
    for (int i = 0; i < 30; ++i) {
        const QPointF d((rng.generateDouble() - 0.5) * 1.6, (rng.generateDouble() - 0.5) * 1.6);
        const double s = 1.0 + rng.bounded(2.0);
        p.drawEllipse(c + d * R, s, s);
    }
    p.restore();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rgb(0x3dff8b), 2.0));
    p.drawPolygon(regular(c, R * 0.18, 8, M_PI / 8));
    p.setPen(QPen(rgb(0x05040f), 3.0));
    p.drawPath(floor);
}

void paintDirt(QPainter &p, const QPainterPath &floor, QPointF c, double R, QRandomGenerator &rng)
{
    // Pas de rebord : le sol se délite dans le vide.
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 44; ++i) {
        const double a = rng.bounded(kTau), r = R * (0.98 + rng.bounded(0.16));
        p.setBrush(QColor(74, 53, 36, 100 + int(rng.bounded(130))));
        p.drawPolygon(regular(c + QPointF(std::cos(a), std::sin(a)) * r, R * (0.012 + rng.bounded(0.028)), 5, rng.bounded(3.0)));
    }
    radialFill(p, floor, c, R, {{0.0, 0x6b4a2e}, {0.7, 0x3b2716}, {1.0, 0x1a1008}}, QPointF(-R * 0.04, -R * 0.05));
    p.save();
    p.setClipPath(floor);
    for (int k = 0; k < 7; ++k) {
        const QPainterPath path = crack(rng, c, R, k * kTau / 7 + rng.bounded(0.4), 0.12, 0.06, 0.5);
        p.strokePath(path, QPen(rgb(0x2a1a0e, 150), 1.2)); // Discrète : à peine plus sombre que le sol.
    }
    p.setPen(QPen(QColor(0x8a, 0x6a, 0x44), 1.0));
    p.setBrush(QColor(0x2a, 0x1a, 0x0e));
    for (int i = 0; i < 18; ++i) {
        const double a = rng.bounded(kTau), r = R * (0.2 + rng.bounded(0.7));
        p.drawPolygon(regular(c + QPointF(std::cos(a), std::sin(a)) * r, R * (0.016 + rng.bounded(0.034)), 5 + int(rng.bounded(3)), rng.bounded(3.0)));
    }
    p.restore();
    QRadialGradient core(c, R * 0.2);
    core.setColorAt(0, rgb(0xb8ff2e, 90));
    core.setColorAt(1, rgb(0xb8ff2e, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(core);
    p.drawEllipse(c, R * 0.2, R * 0.2);
    QPen edge(rgb(0xb8ff2e, 130), 1.5);
    edge.setDashPattern({4.0, 5.0});
    p.setBrush(Qt::NoBrush);
    p.setPen(edge);
    p.drawPath(floor);
}

void paintIce(QPainter &p, const QPainterPath &floor, const StadiumGeometry &g, QPointF c,
              double scale, double R, QRandomGenerator &rng)
{
    const double aspect = g.kind == StadiumGeometry::Kind::Ellipse ? g.aspect : 1.0;
    const QPainterPath rim = outlinePath(g, c, scale, R * 0.1);
    Theme::strokeGlow(p, rim, rgb(0x8ff3ff, 140), 2.0);
    QLinearGradient rimFill(c - QPointF(R, R), c + QPointF(R, R));
    rimFill.setColorAt(0, rgb(0xe6fcff));
    rimFill.setColorAt(0.5, rgb(0x5ab8e0));
    rimFill.setColorAt(1, rgb(0x1a3a7a));
    p.fillPath(rim, rimFill);
    radialFill(p, floor, c, R, {{0.0, 0xc9f6ff}, {0.5, 0x4fa6e8}, {1.0, 0x18286e}}, QPointF(-R * 0.08, -R * 0.1));
    p.save();
    p.setClipPath(floor);
    // Facettes cristallines.
    QList<QPointF> pts;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 12; ++j) {
            const double r = R * (i + 0.5) / 5.6;
            const double a = j * kTau / 12 + i * 0.26 + (rng.generateDouble() - 0.5) * 0.2;
            pts << c + QPointF(std::cos(a) * r, std::sin(a) * r * aspect);
        }
    QPainterPath facets;
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 12; ++j) {
            facets.moveTo(pts[i * 12 + j]);
            facets.lineTo(pts[i * 12 + (j + 1) % 12]);
            facets.lineTo(pts[(i + 1) * 12 + j]);
            facets.closeSubpath();
        }
    p.strokePath(facets, QPen(QColor(255, 255, 255, 85), 0.8));
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 9; ++i) {
        p.setBrush(QColor(255, 255, 255, 14 + int(rng.bounded(16))));
        p.drawPolygon(QPolygonF{pts[int(rng.bounded(60))], pts[int(rng.bounded(60))], c});
    }
    p.save();
    p.translate(c + QPointF(-R * 0.3, -R * 0.25 * aspect));
    p.rotate(-18);
    p.setBrush(QColor(255, 255, 255, 90));
    p.drawEllipse(QPointF(0, 0), R * 0.35, R * 0.08);
    p.restore();
    p.restore();
    TopPainter::paintEmblem(p, QStringLiteral("givre"), c, R * 0.1, Qt::white, 1.8);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rgb(0x05040f), 3.0));
    p.drawPath(floor);
}

void paintSand(QPainter &p, const QPainterPath &floor, const StadiumGeometry &g, QPointF c,
               double scale, double R)
{
    const QPainterPath wall = outlinePath(g, c, scale, R * 0.14);
    QLinearGradient wallFill(c - QPointF(R, R), c + QPointF(R, R));
    wallFill.setColorAt(0, rgb(0xe8b86a));
    wallFill.setColorAt(0.5, rgb(0x9a6a34));
    wallFill.setColorAt(1, rgb(0x4a2e18));
    p.fillPath(wall, wallFill);
    p.setPen(QPen(rgb(0x05040f), 2.0));
    p.drawPath(wall);
    // Glyphes gravés le long du mur.
    const QPainterPath glyphLine = outlinePath(g, c, scale, R * 0.07);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rgb(0x7b3cff, 230), 1.5));
    for (int i = 0; i < 18; ++i) {
        const QPointF at = glyphLine.pointAtPercent((i + 0.5) / 18.0);
        p.drawPolygon(regular(at, R * 0.025, 3, i * 1.3));
    }
    radialFill(p, floor, c, R, {{0.0, 0xf2c98a}, {0.6, 0xc08a4c}, {1.0, 0x6a4422}});
    p.save();
    p.setClipPath(floor);
    // Rides du sable qui épousent la cuvette (resserrées et courbées vers le bord).
    QPainterPath dunes;
    for (int i = -11; i <= 11; ++i) {
        const double y0 = i * R * 0.11;
        constexpr int kSamples = 96;
        for (int k = 0; k <= kSamples; ++k) {
            const double x = -R * 1.3 + k * R * 2.6 / kSamples;
            const double y = y0 + std::sin(x / R * 4.0 * M_PI + i) * R * 0.035;
            const QPointF q = onBowl(c, R, c + QPointF(x, y));
            k == 0 ? dunes.moveTo(q) : dunes.lineTo(q);
        }
    }
    p.strokePath(dunes, QPen(QColor(0x6a, 0x44, 0x22, 130), 1.3));
    p.restore();
    bowlShading(p, floor, c, R);
    p.setPen(QPen(rgb(0x7b3cff, 150), 2.0));
    p.drawPath(outlinePath(g, c, scale, -R * 0.02));
    p.setPen(QPen(rgb(0x7b3cff), 2.0));
    p.drawPolygon(regular(c, R * 0.2, 6, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(rgb(0x7b3cff, 200));
    p.drawPolygon(regular(c, R * 0.1, 3, -M_PI / 2));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rgb(0x05040f), 3.0));
    p.drawPath(floor);
}

void paintVolcano(QPainter &p, const QPainterPath &floor, QPointF c, double R, QRandomGenerator &rng)
{
    QRadialGradient heat(c, R * 1.35);
    heat.setColorAt(0.7, rgb(0xff3d1f, 70));
    heat.setColorAt(1.0, rgb(0xff3d1f, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(heat);
    p.drawEllipse(c, R * 1.35, R * 1.35);
    p.setBrush(rgb(0x2a0e14, 210));
    for (int i = 0; i < 30; ++i) {
        const double a = rng.bounded(kTau), r = R * (1.0 + rng.bounded(0.18));
        p.drawPolygon(regular(c + QPointF(std::cos(a), std::sin(a)) * r, R * (0.015 + rng.bounded(0.03)), 4, rng.bounded(3.0)));
    }
    QRadialGradient bowl(c, R);
    bowl.setColorAt(0.0, rgb(0xff6a3d));
    bowl.setColorAt(0.25, rgb(0x7a1b2a));
    bowl.setColorAt(0.6, rgb(0x2a0a1e));
    bowl.setColorAt(1.0, rgb(0x12040e));
    p.fillPath(floor, bowl);
    p.save();
    p.setClipPath(floor);
    // Courbes de niveau serrées : la pente est raide.
    QPainterPath contours;
    for (int i = 1; i <= 12; ++i) {
        const double r = R * std::pow(i / 13.0, 0.7);
        for (int k = 0; k <= 72; ++k) {
            const double a = k * kTau / 72;
            const double rr = r * (1 + 0.025 * std::sin(a * 5 + i));
            const QPointF q = c + QPointF(std::cos(a), std::sin(a)) * rr;
            if (k == 0)
                contours.moveTo(q);
            else
                contours.lineTo(q);
        }
    }
    p.strokePath(contours, QPen(rgb(0xff6a3d, 90), 0.9));
    for (int k = 0; k < 9; ++k) {
        const QPainterPath lava = crack(rng, c, R, k * kTau / 9 + rng.bounded(0.3), 0.1, 0.07, 0.6);
        p.strokePath(lava, QPen(rgb(0x8a2a1e, 110), 1.2)); // Discrète : braise sourde, pas de halo.
    }
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 22; ++i) {
        const double a = rng.bounded(kTau), r = R * rng.bounded(1.0);
        p.setBrush(rgb(0xffc23d, 100 + int(rng.bounded(150))));
        const double s = 1.0 + rng.bounded(2.0);
        p.drawEllipse(c + QPointF(std::cos(a), std::sin(a)) * r, s, s);
    }
    p.restore();
    QRadialGradient core(c, R * 0.18);
    core.setColorAt(0.0, rgb(0xfff1c2));
    core.setColorAt(0.4, rgb(0xffc23d, 200));
    core.setColorAt(1.0, rgb(0xffc23d, 0));
    p.setBrush(core);
    p.drawEllipse(c, R * 0.18, R * 0.18);
}

QString percent(double multiplier)
{
    const int value = qRound((multiplier - 1.0) * 100.0);
    return value > 0 ? QStringLiteral("+%1 %").arg(value) : QStringLiteral("%1 %").arg(value);
}

QString translated(const char *text)
{
    return QCoreApplication::translate("toupie::StadiumPainter", text);
}

} // namespace

namespace StadiumPainter {

void paint(QPainter &p, const Stadium &stadium, const StadiumGeometry &geometry, QPointF center,
           double scale)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const double radius = geometry.radius * scale;
    const QPainterPath floor = outlinePath(geometry, center, scale);
    QRandomGenerator rng(static_cast<quint32>(qHash(stadium.id, 7)));
    const QString &id = stadium.id;
    if (id == QLatin1String("beton"))
        paintConcrete(p, floor, geometry, center, scale, radius, rng);
    else if (id == QLatin1String("terre"))
        paintDirt(p, floor, center, radius, rng);
    else if (id == QLatin1String("glace"))
        paintIce(p, floor, geometry, center, scale, radius, rng);
    else if (id == QLatin1String("sable"))
        paintSand(p, floor, geometry, center, scale, radius);
    else if (id == QLatin1String("volcan"))
        paintVolcano(p, floor, center, radius, rng);
    else
        paintClassic(p, floor, geometry, center, scale, radius);
    p.restore();
}

QString describeTraits(const Stadium &stadium)
{
    QStringList traits;
    switch (stadium.shape) {
    case StadiumGeometry::Kind::Circle:
        traits << translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "Cercle"));
        break;
    case StadiumGeometry::Kind::Polygon:
        traits << (stadium.sides == 6   ? translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "Hexagone"))
                   : stadium.sides == 8 ? translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "Octogone"))
                                        : translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "Polygone")));
        break;
    case StadiumGeometry::Kind::Ellipse:
        traits << translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "Ellipse"));
        break;
    }
    if (stadium.size < 0.99)
        traits << translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "petit"));
    if (!stadium.hasRim)
        traits << translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "sans rebord"));
    else if (stadium.rimHeight > 1.05)
        traits << translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "rebord haut"));
    else if (stadium.rimHeight < 0.95)
        traits << translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "rebord bas"));
    else
        traits << translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "rebord"));

    auto add = [&](const QString &label, double value) {
        if (std::abs(value - 1.0) > 0.01)
            traits << QStringLiteral("%1 %2").arg(label, percent(value));
    };
    add(translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "vitesse")), stadium.speed);
    add(translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "frottement")), stadium.friction);
    add(translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "pente")), stadium.slope);
    add(translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "usure")), stadium.enduranceDrain);
    add(translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "dégâts")), stadium.damage);
    add(translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "recul")), stadium.knockback);
    add(translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "portée du dash")), stadium.dashSpeed);
    add(translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "puissance du dash")), stadium.dashPower);
    if (traits.size() == 2)
        traits << translated(QT_TRANSLATE_NOOP("toupie::StadiumPainter", "aucun modificateur"));
    return traits.join(QStringLiteral(" · "));
}

} // namespace StadiumPainter

StadiumWidget::StadiumWidget(QWidget *parent)
    : QWidget(parent)
    , m_stadium(Stadium::classic())
{
}

void StadiumWidget::setStadium(const Stadium &stadium)
{
    m_stadium = stadium;
    update();
}

void StadiumWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double thumb = qMin(height() - 12.0, 120.0);
    const QPointF center(8 + thumb / 2.0, height() / 2.0);
    BattleConfig config = m_stadium.apply(BattleConfig{});
    const StadiumGeometry geometry = m_stadium.geometry(config);
    StadiumPainter::paint(p, m_stadium, geometry, center, thumb / 2.0 * 0.78);

    const QRectF text(thumb + 26, 4, width() - thumb - 30, height() - 8);
    const QColor accent = QColor::fromRgb(m_stadium.accent);
    QFont title = Theme::displayFont(19);
    title.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    p.setFont(title);
    p.setPen(Theme::kText);
    const QString name = m_stadium.name.toUpper();
    const double titleHeight = p.fontMetrics().height() + 2;
    p.drawText(QRectF(text.left(), text.top(), text.width(), titleHeight),
               Qt::AlignLeft | Qt::AlignVCenter, name);
    if (!m_stadium.codename.isEmpty()) {
        QFont code = Theme::displayFont(13);
        code.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
        const double nameWidth = p.fontMetrics().horizontalAdvance(name);
        p.setFont(code);
        p.setPen(accent);
        p.drawText(QRectF(text.left() + nameWidth + 14, text.top(), text.width(), titleHeight),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("// %1").arg(m_stadium.codename.toUpper()));
    }

    p.setFont(Theme::bodyFont(15, QFont::Medium));
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    p.setPen(Theme::kMuted);
    const QRectF body(text.left(), text.top() + titleHeight + 2, text.width(),
                      text.height() - titleHeight - 26);
    p.drawText(body, m_stadium.description, option);
    p.setFont(Theme::bodyFont(14, QFont::Bold));
    p.setPen(accent);
    p.drawText(QRectF(text.left(), text.bottom() - 22, text.width(), 22),
               Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(StadiumPainter::describeTraits(m_stadium), Qt::ElideRight,
                                          int(text.width())));
}

} // namespace toupie
