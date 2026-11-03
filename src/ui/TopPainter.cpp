#include "ui/TopPainter.h"

#include "core/Top.h"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

namespace toupie::TopPainter {

QColor typeColor(TopType type)
{
    switch (type) {
    case TopType::Attack:
        return QColor(226, 67, 60);
    case TopType::Defense:
        return QColor(52, 120, 220);
    case TopType::Endurance:
        return QColor(46, 170, 100);
    case TopType::Balance:
        return QColor(230, 175, 40);
    }
    return Qt::gray;
}

void paint(QPainter &painter, const Top &top, QPointF center, double radius, double angle,
           double glow)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(center);

    const QColor accent = typeColor(top.type());

    if (glow > 0.0) {
        QRadialGradient halo(QPointF(0, 0), radius * 1.8);
        QColor c = accent.lighter(130);
        c.setAlphaF(0.55 * glow);
        halo.setColorAt(0.5, c);
        c.setAlphaF(0.0);
        halo.setColorAt(1.0, c);
        painter.setPen(Qt::NoPen);
        painter.setBrush(halo);
        painter.drawEllipse(QPointF(0, 0), radius * 1.8, radius * 1.8);
    }

    // Ombre portée.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 70));
    painter.drawEllipse(QPointF(radius * 0.12, radius * 0.16), radius, radius);

    painter.rotate(qRadiansToDegrees(angle));

    // Roue métallique : anneau extérieur avec des lames.
    const QColor wheelColor = QColor::fromHsv(top.wheel().hue, 90, 210);
    QRadialGradient metal(QPointF(-radius * 0.3, -radius * 0.3), radius * 1.4);
    metal.setColorAt(0.0, wheelColor.lighter(150));
    metal.setColorAt(1.0, wheelColor.darker(170));
    painter.setBrush(metal);
    painter.setPen(QPen(accent.darker(150), qMax(1.0, radius * 0.08)));
    painter.drawEllipse(QPointF(0, 0), radius, radius);

    painter.setPen(Qt::NoPen);
    painter.setBrush(wheelColor.darker(200));
    const int blades = 3;
    for (int i = 0; i < blades; ++i) {
        painter.save();
        painter.rotate(i * 360.0 / blades);
        QPainterPath blade;
        blade.moveTo(radius * 0.35, -radius * 0.10);
        blade.quadTo(radius * 0.85, -radius * 0.45, radius * 0.98, 0.0);
        blade.quadTo(radius * 0.75, radius * 0.05, radius * 0.35, radius * 0.12);
        blade.closeSubpath();
        painter.drawPath(blade);
        painter.restore();
    }

    // Partie intermédiaire.
    const QColor middleColor = QColor::fromHsv(top.middle().hue, 170, 200);
    painter.setBrush(middleColor);
    painter.setPen(QPen(middleColor.darker(160), qMax(1.0, radius * 0.05)));
    painter.drawEllipse(QPointF(0, 0), radius * 0.48, radius * 0.48);

    // Repère pour voir la rotation.
    painter.setPen(QPen(Qt::white, qMax(1.0, radius * 0.08), Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(radius * 0.12, 0), QPointF(radius * 0.42, 0));

    // Pointe (au centre).
    const QColor tipColor = QColor::fromHsv(top.tip().hue, 200, 230);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tipColor);
    painter.drawEllipse(QPointF(0, 0), radius * 0.16, radius * 0.16);

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
