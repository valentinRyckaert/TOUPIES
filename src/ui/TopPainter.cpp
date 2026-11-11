#include "ui/TopPainter.h"

#include "core/Top.h"
#include "game/SpecialMove.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTransform>
#include <QtMath>

#include <array>
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

/// Contour d'une roue de rayon 1 (mis en cache) ; la roue horaire est en miroir, pour que
/// le côté tranchant de ses lames soit à l'avant dans le sens de rotation.
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

QPolygonF polygon(std::initializer_list<QPointF> points)
{
    return QPolygonF(points);
}

/// Bande d'anneau entre les rayons r1 et r2 (degrés, sens de Qt).
QPainterPath arcBand(double r1, double r2, double start, double span)
{
    QPainterPath p;
    const QRectF outer(-r2, -r2, 2 * r2, 2 * r2), inner(-r1, -r1, 2 * r1, 2 * r1);
    p.arcMoveTo(outer, start);
    p.arcTo(outer, start, span);
    p.arcTo(inner, start + span, -span);
    p.closeSubpath();
    return p;
}

/// Emblème abstrait de chaque coup (formes pleines dans [-1, 1]²), sauf la lance.
QPainterPath emblemPath(const QString &special)
{
    QPainterPath p;
    p.setFillRule(Qt::WindingFill);
    if (special == QLatin1String("onde")) { // Ondes en croissants autour d'un noyau.
        p.addEllipse(QPointF(0, 0), 0.2, 0.2);
        p.addPath(arcBand(0.36, 0.52, 110, 290));
        p.addPath(arcBand(0.66, 0.78, -40, 240));
        p.addPath(arcBand(0.88, 0.97, 150, 130));
    } else if (special == QLatin1String("givre")) { // Cristal : six losanges inégaux.
        for (int k = 0; k < 6; ++k) {
            const double a = k * M_PI / 3 - M_PI / 2, len = k % 2 ? 0.62 : 1.0, w = k % 2 ? 0.13 : 0.17;
            const QPointF d(std::cos(a), std::sin(a)), n(-d.y(), d.x());
            QPainterPath shard;
            shard.addPolygon(polygon({d * 0.1, d * len * 0.55 + n * w, d * len, d * len * 0.55 - n * w}));
            shard.closeSubpath();
            p = p.united(shard);
        }
    } else if (special == QLatin1String("vortex")) { // Trois lames en spirale.
        for (int k = 0; k < 3; ++k) {
            QPolygonF blade;
            constexpr int kSteps = 24;
            for (int i = 0; i <= kSteps; ++i) { // Bord extérieur…
                const double t = double(i) / kSteps, a = k * kTau / 3 + t * 2.3;
                blade << QPointF(std::cos(a), std::sin(a)) * (0.12 + 0.86 * t);
            }
            for (int i = kSteps; i >= 0; --i) { // … puis intérieur, effilé aux deux bouts.
                const double t = double(i) / kSteps, a = k * kTau / 3 + t * 2.3 - 0.25 * std::sin(M_PI * t);
                blade << QPointF(std::cos(a), std::sin(a)) * (0.12 + 0.86 * t - 0.32 * std::sin(M_PI * t));
            }
            p.addPolygon(blade);
            p.closeSubpath();
        }
    } else if (special == QLatin1String("distorsion")) { // Œil découpé en bandes décalées.
        QPainterPath eye(QPointF(-0.95, 0));
        eye.cubicTo(-0.45, -0.95, 0.45, -0.95, 0.95, 0);
        eye.cubicTo(0.45, 0.95, -0.45, 0.95, -0.95, 0);
        QPainterPath pupil;
        pupil.addEllipse(QPointF(0, 0), 0.3, 0.3);
        eye = eye.subtracted(pupil);
        const double cuts[4] = {-0.8, -0.2, 0.16, 0.8}, shifts[3] = {-0.18, 0.14, -0.08};
        for (int i = 0; i < 3; ++i) {
            QPainterPath band;
            band.addRect(QRectF(-1.2, cuts[i] + 0.035, 2.4, cuts[i + 1] - cuts[i] - 0.07));
            p.addPath(eye.intersected(band).translated(shifts[i], 0));
        }
        p.addEllipse(QPointF(0.14, 0), 0.13, 0.13);
    } else if (special == QLatin1String("siphon")) { // Deux crocs et une goutte.
        for (double side : {-1.0, 1.0}) {
            QPainterPath fang(QPointF(side * 0.78, -0.62));
            fang.cubicTo(side * 0.72, -0.05, side * 0.45, 0.45, side * 0.16, 0.85);
            fang.cubicTo(side * 0.28, 0.35, side * 0.34, -0.15, side * 0.36, -0.62);
            fang.closeSubpath();
            p.addPath(fang);
        }
        QPainterPath drop(QPointF(0, -0.72));
        drop.cubicTo(0.12, -0.5, 0.2, -0.34, 0.2, -0.22);
        drop.arcTo(QRectF(-0.2, -0.42, 0.4, 0.4), 0, -180);
        drop.cubicTo(-0.2, -0.34, -0.12, -0.5, 0, -0.72);
        p.addPath(drop);
    } else if (special == QLatin1String("seisme")) { // Plaques séparées par une faille.
        p.addPolygon(polygon({{-0.95, 0.62}, {-0.55, -0.3}, {-0.16, -0.02}, {-0.34, 0.62}}));
        p.closeSubpath();
        p.addPolygon(polygon({{-0.22, 0.62}, {0.0, -0.8}, {0.36, -0.12}, {0.16, 0.62}}));
        p.closeSubpath();
        p.addPolygon(polygon({{0.28, 0.62}, {0.47, 0.02}, {0.95, 0.62}}));
        p.closeSubpath();
    } else {
        p.addEllipse(QPointF(0, 0), 0.4, 0.4);
    }
    return p;
}

/// Lance d'acier : dague à lame biseautée (une face claire, une sombre), arête et éclat.
void paintDagger(QPainter &p, const QColor &color, double line)
{
    const QPen edge(QColor(0x14, 0x18, 0x40), line);
    QLinearGradient light(-0.18, -0.9, 0.0, 0.4);
    light.setColorAt(0.0, Qt::white);
    light.setColorAt(1.0, QColor(0xb8, 0xc6, 0xff));
    QLinearGradient dark(0.0, -0.9, 0.18, 0.4);
    dark.setColorAt(0.0, QColor(0xa8, 0xb4, 0xf0));
    dark.setColorAt(1.0, QColor(0x3a, 0x45, 0x90));
    p.setPen(Qt::NoPen);
    p.setBrush(light);
    p.drawPolygon(polygon({{0, -0.98}, {0, 0.36}, {-0.13, 0.36}, {-0.18, -0.22}}));
    p.setBrush(dark);
    p.drawPolygon(polygon({{0, -0.98}, {0.18, -0.22}, {0.13, 0.36}, {0, 0.36}}));
    p.setBrush(Qt::NoBrush);
    p.setPen(edge);
    p.drawPolygon(polygon({{0, -0.98}, {0.18, -0.22}, {0.13, 0.36}, {-0.13, 0.36}, {-0.18, -0.22}}));
    p.setPen(QPen(QColor(255, 255, 255, 230), line * 0.8, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(0, -0.9), QPointF(0, 0.3)); // Arête.

    // Garde incurvée, poignée et pommeau, teintés de la couleur du coup.
    QPainterPath guard(QPointF(-0.5, 0.3));
    guard.quadTo(0, 0.5, 0.5, 0.3);
    guard.lineTo(0.46, 0.42);
    guard.quadTo(0, 0.6, -0.46, 0.42);
    guard.closeSubpath();
    QLinearGradient tint(0, 0.3, 0, 0.6);
    tint.setColorAt(0.0, color.lighter(130));
    tint.setColorAt(1.0, color.darker(160));
    p.setPen(edge);
    p.setBrush(tint);
    p.drawPath(guard);
    p.setBrush(QColor(0x26, 0x2a, 0x5a));
    p.drawRect(QRectF(-0.07, 0.5, 0.14, 0.28));
    p.setBrush(tint);
    p.drawPolygon(polygon({{0, 0.76}, {0.12, 0.88}, {0, 1.0}, {-0.12, 0.88}}));

    // Éclat à quatre branches près de la pointe.
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    const QPointF g(-0.08, -0.58);
    p.drawPolygon(polygon({g + QPointF(0, -0.2), g + QPointF(0.035, -0.035), g + QPointF(0.2, 0), g + QPointF(0.035, 0.035),
                           g + QPointF(0, 0.2), g + QPointF(-0.035, 0.035), g + QPointF(-0.2, 0), g + QPointF(-0.035, -0.035)}));
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

void paintBolt(QPainter &p, const QString &special, double R, double rot)
{
    const QColor col = moveColor(special);
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
    if (R >= 6.0) {
        // L'emblème tourne avec la toupie (seul le reflet, plus bas, reste fixe).
        p.save();
        p.rotate(qRadiansToDegrees(rot));
        paintEmblem(p, special, QPointF(0, 0), R * 0.5, Qt::white, std::max(0.8, R * 0.08));
        p.restore();
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 90));
    p.save();
    p.translate(-R * 0.28, -R * 0.36);
    p.rotate(-35);
    p.drawEllipse(QPointF(0, 0), R * 0.26, R * 0.12);
    p.restore();
}


QLinearGradient horizontal(double left, double right, std::initializer_list<std::pair<double, unsigned>> stops)
{
    QLinearGradient g(left, 0, right, 0);
    for (const auto &[at, rgb] : stops)
        g.setColorAt(at, QColor::fromRgb(rgb));
    return g;
}

/// Pointe vue de profil : collerette métallique puis embout propre à chaque pointe ;
/// `y` est le haut de la collerette, `W` sa largeur.
void paintTip(QPainter &p, const QString &id, double cx, double y, double W)
{
    const double s = W / 2.0;
    const QLinearGradient metal = horizontal(cx - s, cx + s, {{0.0, 0x5c649e}, {0.45, 0xeef2ff}, {1.0, 0x3a3f72}});
    const QLinearGradient rubber = horizontal(cx - s, cx + s, {{0.0, 0x3a0f2a}, {0.5, 0xff3d6e}, {1.0, 0x3a0f2a}});
    auto poly = [&](std::initializer_list<QPointF> pts) {
        QPolygonF q;
        for (const QPointF &pt : pts)
            q << QPointF(cx + pt.x() * s, y + pt.y() * s);
        return q;
    };
    auto ball = [&](double cy, double r, bool shaded) {
        QRadialGradient g(QPointF(cx - r * 0.3, cy - r * 0.4), r * 1.4);
        g.setColorAt(0.0, Qt::white);
        if (shaded)
            g.setColorAt(0.5, QColor(0x8e, 0x98, 0xc8));
        g.setColorAt(1.0, QColor(0x2a, 0x2f, 0x60));
        p.setBrush(g);
        p.drawEllipse(QPointF(cx, cy), r, r);
    };
    p.setPen(QPen(QColor(0x05, 0x04, 0x0f), std::max(0.6, W * 0.017)));
    p.setBrush(metal);
    p.drawPolygon(poly({{-1, 0}, {1, 0}, {0.8, 0.35}, {-0.8, 0.35}})); // Collerette.
    if (id == QLatin1String("P1")) {
        p.drawPolygon(poly({{-0.55, 0.35}, {0.55, 0.35}, {0.18, 1.1}, {-0.18, 1.1}}));
    } else if (id == QLatin1String("P2")) {
        p.setBrush(rubber);
        p.drawPolygon(poly({{-0.7, 0.35}, {0.7, 0.35}, {0.62, 0.95}, {-0.62, 0.95}}));
    } else if (id == QLatin1String("P3")) {
        p.drawPolygon(poly({{-0.35, 0.35}, {0.35, 0.35}, {0, 1.45}}));
    } else if (id == QLatin1String("P4")) {
        ball(y + s * 0.82, s * 0.5, true);
    } else if (id == QLatin1String("P5")) {
        p.drawPolygon(poly({{-0.6, 0.35}, {0.6, 0.35}, {0.85, 0.9}, {-0.85, 0.9}}));
    } else if (id == QLatin1String("P6")) {
        p.drawPolygon(poly({{-0.45, 0.35}, {0.45, 0.35}, {0.05, 1.3}, {-0.05, 1.3}}));
    } else if (id == QLatin1String("P7")) {
        p.setBrush(rubber);
        p.drawPolygon(poly({{-0.7, 0.35}, {0.7, 0.35}, {0.6, 0.8}, {-0.6, 0.8}}));
        p.setBrush(QColor(0xff, 0x3d, 0x6e));
        for (int i = -2; i <= 2; ++i)
            p.drawPolygon(poly({{i * 0.25 - 0.08, 0.8}, {i * 0.25 + 0.08, 0.8}, {i * 0.25, 1.05}}));
    } else if (id == QLatin1String("P8")) {
        QPainterPath dome(QPointF(cx - s * 0.75, y + s * 0.35));
        dome.arcTo(QRectF(cx - s * 0.75, y - s * 0.35, s * 1.5, s * 1.4), 180, 180);
        dome.closeSubpath();
        p.drawPath(dome);
    } else if (id == QLatin1String("P9")) {
        p.drawPolygon(poly({{-0.45, 0.35}, {0.45, 0.35}, {0.3, 0.6}, {-0.3, 0.6}}));
        ball(y + s * 0.85, s * 0.3, false);
    } else { // Sabot (et pointe inconnue)
        p.drawPolygon(poly({{-0.6, 0.35}, {0.6, 0.35}, {0.5, 0.8}, {0.25, 1.05}, {-0.25, 1.05}, {-0.5, 0.8}}));
    }
    p.setPen(QPen(QColor(255, 255, 255, 100), 1.0));
    p.drawLine(QPointF(cx - s * 0.9, y + s * 0.06), QPointF(cx + s * 0.9, y + s * 0.06));
}

/// Roue seule vue du dessus (carte de l'atelier), centrée sur l'origine.
void paintWheel(QPainter &p, const Part &wheel, double R)
{
    const bool mirror = wheel.spin == SpinDirection::Clockwise; // Pointes des lames vers l'avant.
    const QColor accent = hsl(wheel.hue, 0.95, 0.6);
    const QTransform t = QTransform().rotateRadians(-M_PI / 2).scale(R, R);
    const QPainterPath silhouette = t.map(unitWheel(wheel.id, mirror, 1.0));
    QRadialGradient metal(QPointF(-R * 0.3, -R * 0.4), R * 1.6);
    metal.setColorAt(0.0, QColor(0xf4, 0xf7, 0xff));
    metal.setColorAt(0.5, QColor(0x9a, 0xa4, 0xd8));
    metal.setColorAt(1.0, QColor(0x1d, 0x21, 0x50));
    p.setBrush(metal);
    p.setPen(QPen(QColor(0x05, 0x04, 0x0f), std::max(0.6, R * 0.03)));
    p.drawPath(silhouette);
    QLinearGradient tint(QPointF(-R, -R), QPointF(R, R));
    tint.setColorAt(0.0, hsl(wheel.hue, 0.95, 0.6, 0.85));
    tint.setColorAt(0.5, hsl(wheel.hue, 0.8, 0.22, 0.6));
    tint.setColorAt(1.0, hsl(wheel.hue, 0.95, 0.6, 0.9));
    p.fillPath(silhouette, tint);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 115), 1.0));
    p.drawPath(t.map(unitWheel(wheel.id, mirror, 0.86)));
    p.setBrush(QColor(0x0a, 0x08, 0x26));
    p.setPen(QPen(accent, std::max(1.0, R * 0.045)));
    p.drawEllipse(QPointF(0, 0), R * 0.6, R * 0.6);
    QPen dashed(QColor(0x3a, 0x2f, 0x8f), 1.0);
    dashed.setDashPattern({3, 3});
    p.setPen(dashed);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(0, 0), R * 0.25, R * 0.25);
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
    painter.translate(center);
    painter.scale(size, size);
    const double line = std::max(0.5, penWidth * 0.5) / size; // Liseré en pixels.
    if (special == QLatin1String("lance")) {
        paintDagger(painter, color, line);
    } else {
        // Forme pleine : dégradé de la couleur, liseré plus sombre.
        QLinearGradient fill(-1, -1, 1, 1);
        fill.setColorAt(0.0, color.lighter(115));
        QColor deep = color.darker(140);
        deep.setAlphaF(0.9);
        fill.setColorAt(1.0, deep);
        painter.setBrush(fill);
        QColor rim = color.darker(220);
        rim.setAlphaF(0.7);
        painter.setPen(QPen(rim, line, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(emblemPath(special));
    }
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
    const bool mirror = wheel.spin == SpinDirection::Clockwise; // Pointes des lames vers l'avant.
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
        paintBolt(painter, top.special(), radius * 0.34, angle);
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

void paintPart(QPainter &painter, const Part &part, QPointF center, double radius)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(center);
    switch (part.kind) {
    case PartKind::Bolt: {
        QRadialGradient glow(QPointF(0, 0), radius * 1.2);
        QColor c = moveColor(part.special);
        c.setAlpha(90);
        glow.setColorAt(0.5, c);
        c.setAlpha(0);
        glow.setColorAt(1.0, c);
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(QPointF(0, 0), radius * 1.2, radius * 1.2);
        paintBolt(painter, part.special, radius, 0.0);
        break;
    }
    case PartKind::Wheel:
        paintWheel(painter, part, radius);
        break;
    case PartKind::Middle:
        paintMiddle(painter, part, radius, 0.0);
        break;
    case PartKind::Tip:
        paintTip(painter, part.id, 0.0, -radius * 0.7, radius * 1.9);
        break;
    }
    painter.restore();
}

std::array<QPointF, 4> paintProfile(QPainter &p, const Top &top, QPointF origin, double W)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const double cx = origin.x(), gap = W * 0.16;
    double y = origin.y();
    std::array<QPointF, 4> anchors;
    const auto anchor = [&anchors](PartKind kind, QPointF at) { anchors[static_cast<int>(kind)] = at; };
    const QPen edge(QColor(0x05, 0x04, 0x0f), 1.0);
    const QLinearGradient metal = horizontal(cx - W * 0.5, cx + W * 0.5,
                                             {{0.0, 0x3a3f72}, {0.35, 0xeef2ff}, {0.6, 0x9aa4d8}, {1.0, 0x262a5a}});

    // Boulon : dôme métallique et pastille de la couleur du coup.
    QPainterPath dome(QPointF(cx - W * 0.16, y + W * 0.12));
    dome.quadTo(QPointF(cx - W * 0.15, y), QPointF(cx, y));
    dome.quadTo(QPointF(cx + W * 0.15, y), QPointF(cx + W * 0.16, y + W * 0.12));
    dome.closeSubpath();
    p.setPen(edge);
    p.setBrush(metal);
    p.drawPath(dome);
    const QColor move = moveColor(top.special());
    QRadialGradient glow(QPointF(cx, y + W * 0.05), W * 0.12);
    glow.setColorAt(0.0, move);
    glow.setColorAt(1.0, QColor(move.red(), move.green(), move.blue(), 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(QPointF(cx, y + W * 0.05), W * 0.12, W * 0.06);
    p.setBrush(move);
    p.drawEllipse(QPointF(cx, y + W * 0.05), W * 0.07, W * 0.03);
    anchor(PartKind::Bolt, QPointF(cx, y + W * 0.03));
    y += W * 0.12 + gap;

    // Roue : bandeau dont la largeur suit le profil de la silhouette.
    const Part &wheel = top.wheel();
    const Profile r = wheelProfile(wheel.id);
    double lo = 1.0, hi = 0.0;
    for (int i = 0; i <= 40; ++i) {
        lo = std::min(lo, r(i * M_PI / 40));
        hi = std::max(hi, r(i * M_PI / 40));
    }
    const double rim = W * 0.14;
    const QPolygonF band{{cx - W * 0.42, y}, {cx + W * 0.42, y}, {cx + W * 0.5 * hi, y + rim * 0.35},
                         {cx + W * 0.5 * lo, y + rim}, {cx - W * 0.5 * lo, y + rim}, {cx - W * 0.5 * hi, y + rim * 0.35}};
    p.setPen(edge);
    p.setBrush(metal);
    p.drawPolygon(band);
    p.setPen(Qt::NoPen);
    p.setBrush(hsl(wheel.hue, 0.95, 0.6, 0.45));
    p.drawPolygon(band);
    p.setPen(QPen(QColor(255, 255, 255, 150), 1.0));
    p.drawLine(QPointF(cx - W * 0.42, y + 2), QPointF(cx + W * 0.42, y + 2));
    p.fillRect(QRectF(cx - W * 0.3, y + rim * 0.45, W * 0.6, rim * 0.18), hsl(wheel.hue, 0.95, 0.6, 0.9));
    anchor(PartKind::Wheel, QPointF(cx + W * 0.375, y + rim * 0.2));
    y += rim + gap;

    // Partie intermédiaire : disque bombé et liseré de sa teinte.
    p.setPen(edge);
    p.setBrush(horizontal(cx - W * 0.3, cx + W * 0.3, {{0.0, 0x141038}, {0.5, 0x3b3490}, {1.0, 0x141038}}));
    p.drawPolygon(QPolygonF{{cx - W * 0.3, y}, {cx + W * 0.3, y}, {cx + W * 0.26, y + W * 0.09}, {cx - W * 0.26, y + W * 0.09}});
    p.fillRect(QRectF(cx - W * 0.27, y + W * 0.035, W * 0.54, W * 0.02), hsl(top.middle().hue, 0.95, 0.62));
    anchor(PartKind::Middle, QPointF(cx - W * 0.28, y + W * 0.045));
    y += W * 0.09 + gap;

    paintTip(p, top.tip().id, cx, y, W * 0.24);
    anchor(PartKind::Tip, QPointF(cx + W * 0.06, y + W * 0.1));
    p.restore();
    return anchors;
}

} // namespace toupie::TopPainter
