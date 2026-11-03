#include "ui/StadiumPainter.h"

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

QColor rgb(unsigned value)
{
    return QColor::fromRgb(value);
}

/// Motif de surface propre à chaque stadium (déterministe).
void paintTexture(QPainter &p, const Stadium &stadium, const QPainterPath &floor, QPointF center,
                  double radius)
{
    p.save();
    p.setClipPath(floor);
    QRandomGenerator rng(static_cast<quint32>(qHash(stadium.id, 7)));
    auto randomPoint = [&] {
        const double a = rng.bounded(2.0 * M_PI);
        const double r = std::sqrt(rng.bounded(1.0)) * radius;
        return center + QPointF(std::cos(a), std::sin(a)) * r;
    };

    if (stadium.id == QLatin1String("beton")) {
        // Fissures.
        p.setPen(QPen(QColor(60, 62, 66, 90), 1.2));
        for (int i = 0; i < 14; ++i) {
            QPointF a = randomPoint();
            QPainterPath crack(a);
            for (int k = 0; k < 4; ++k) {
                a += QPointF(rng.bounded(radius * 0.16) - radius * 0.08,
                             rng.bounded(radius * 0.16) - radius * 0.08);
                crack.lineTo(a);
            }
            p.drawPath(crack);
        }
    } else if (stadium.id == QLatin1String("terre") || stadium.id == QLatin1String("sable")) {
        // Grains.
        const bool sand = stadium.id == QLatin1String("sable");
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 420; ++i) {
            QColor c = sand ? QColor(150, 110, 50) : QColor(70, 45, 20);
            c.setAlpha(40 + int(rng.bounded(60)));
            p.setBrush(c);
            const double size = radius * (0.004 + rng.bounded(0.008));
            p.drawEllipse(randomPoint(), size, size);
        }
        if (sand) {
            // Ondulations du sable.
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 245, 210, 70), 2.0));
            for (int i = 1; i <= 6; ++i)
                p.drawEllipse(center, radius * i / 7.0, radius * i / 7.5);
        }
    } else if (stadium.id == QLatin1String("glace")) {
        // Reflets.
        p.setPen(QPen(QColor(255, 255, 255, 140), 2.0, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 9; ++i) {
            const QPointF a = randomPoint();
            p.drawLine(a, a + QPointF(radius * 0.18, -radius * 0.10));
        }
    } else if (stadium.id == QLatin1String("volcan")) {
        // Veines de lave.
        p.setPen(QPen(QColor(255, 110, 40, 120), 1.6));
        for (int i = 0; i < 10; ++i) {
            QPointF a = randomPoint();
            QPainterPath vein(a);
            for (int k = 0; k < 5; ++k) {
                a += QPointF(rng.bounded(radius * 0.12) - radius * 0.06,
                             rng.bounded(radius * 0.12) - radius * 0.06);
                vein.lineTo(a);
            }
            p.drawPath(vein);
        }
    } else {
        // Classique : cercles concentriques.
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 60), 1.0));
        for (double f : {0.33, 0.66})
            p.drawEllipse(center, radius * f, radius * f);
    }
    p.restore();
}

QString percent(double multiplier)
{
    const int value = qRound((multiplier - 1.0) * 100.0);
    return value > 0 ? QStringLiteral("+%1 %").arg(value) : QStringLiteral("%1 %").arg(value);
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

    if (stadium.hasRim) {
        // Rebord : bande épaisse autour du sol.
        const QPainterPath rim = outlinePath(geometry, center, scale, radius * 0.06);
        p.setPen(Qt::NoPen);
        p.setBrush(rgb(stadium.rimColor));
        p.drawPath(rim);
    } else {
        // Pas de rebord : halo de « vide » (ou de lave) autour du sol.
        QColor glow = rgb(stadium.rimColor);
        for (int i = 4; i >= 1; --i) {
            glow.setAlpha(28 * (5 - i));
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawPath(outlinePath(geometry, center, scale, radius * 0.025 * i));
        }
    }

    QRadialGradient bowl(center, radius);
    bowl.setColorAt(0.0, rgb(stadium.floorCenter));
    bowl.setColorAt(0.75, rgb(stadium.floorCenter).darker(125));
    bowl.setColorAt(1.0, rgb(stadium.floorEdge));
    p.setBrush(bowl);
    p.setPen(Qt::NoPen);
    p.drawPath(floor);

    paintTexture(p, stadium, floor, center, radius);

    p.setBrush(Qt::NoBrush);
    if (stadium.hasRim) {
        p.setPen(QPen(rgb(stadium.rimColor).darker(140), 2.0));
    } else {
        p.setPen(QPen(rgb(stadium.rimColor).lighter(130), 2.0, Qt::DashLine));
    }
    p.drawPath(floor);
    p.restore();
}

QString describeTraits(const Stadium &stadium)
{
    QStringList traits;
    switch (stadium.shape) {
    case StadiumGeometry::Kind::Circle:
        traits << QStringLiteral("Cercle");
        break;
    case StadiumGeometry::Kind::Polygon:
        traits << (stadium.sides == 6 ? QStringLiteral("Hexagone")
                   : stadium.sides == 8 ? QStringLiteral("Octogone")
                                        : QStringLiteral("Polygone"));
        break;
    case StadiumGeometry::Kind::Ellipse:
        traits << QStringLiteral("Ellipse");
        break;
    }
    if (stadium.size < 0.99)
        traits << QStringLiteral("petit");
    if (!stadium.hasRim)
        traits << QStringLiteral("sans rebord");
    else if (stadium.rimHeight > 1.05)
        traits << QStringLiteral("rebord haut");
    else if (stadium.rimHeight < 0.95)
        traits << QStringLiteral("rebord bas");
    else
        traits << QStringLiteral("rebord");

    auto add = [&](const QString &label, double value) {
        if (std::abs(value - 1.0) > 0.01)
            traits << QStringLiteral("%1 %2").arg(label, percent(value));
    };
    add(QStringLiteral("vitesse"), stadium.speed);
    add(QStringLiteral("frottement"), stadium.friction);
    add(QStringLiteral("pente"), stadium.slope);
    add(QStringLiteral("usure"), stadium.enduranceDrain);
    add(QStringLiteral("dégâts"), stadium.damage);
    add(QStringLiteral("recul"), stadium.knockback);
    add(QStringLiteral("portée du dash"), stadium.dashSpeed);
    add(QStringLiteral("puissance du dash"), stadium.dashPower);
    if (traits.size() == 2)
        traits << QStringLiteral("aucun modificateur");
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

    const double thumb = qMin(height() - 12.0, 110.0);
    const QPointF center(6 + thumb / 2.0, height() / 2.0);
    BattleConfig config = m_stadium.apply(BattleConfig{});
    const StadiumGeometry geometry = m_stadium.geometry(config);
    StadiumPainter::paint(p, m_stadium, geometry, center, thumb / 2.0 * 0.85);

    const QRectF text(thumb + 20, 4, width() - thumb - 24, height() - 8);
    QFont bold = font();
    bold.setBold(true);
    bold.setPointSizeF(bold.pointSizeF() * 1.1);
    p.setFont(bold);
    p.setPen(palette().color(QPalette::WindowText));
    const double titleHeight = p.fontMetrics().height() + 2;
    p.drawText(QRectF(text.left(), text.top(), text.width(), titleHeight),
               Qt::AlignLeft | Qt::AlignVCenter, m_stadium.name);

    p.setFont(font());
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    const QString body = m_stadium.description + QStringLiteral("\n")
                       + StadiumPainter::describeTraits(m_stadium);
    p.drawText(QRectF(text.left(), text.top() + titleHeight, text.width(),
                      text.height() - titleHeight),
               body, option);
}

} // namespace toupie
