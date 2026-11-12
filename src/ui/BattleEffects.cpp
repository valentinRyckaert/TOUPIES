#include "ui/BattleEffects.h"

#include "ui/Theme.h"
#include "ui/TopPainter.h"

#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace toupie {

namespace {
constexpr int kTrailLength = 36;
constexpr double kSparkLifetime = 0.4;
constexpr double kEmberLifetime = 0.35;

QColor withAlpha(QColor c, double a)
{
    c.setAlphaF(std::clamp(a, 0.0, 1.0));
    return c;
}

double pseudoRandom(int seed)
{
    const double x = std::sin(seed * 12.9898) * 43758.5453;
    return x - std::floor(x);
}
} // namespace

void BattleEffects::reset()
{
    m_trails.clear();
    m_sparks.clear();
    m_embers.clear();
}

void BattleEffects::update(double dt, const QList<ImpactEvent> &impacts,
                           const std::vector<Spinner> &spinners, const Duel &duel, bool moving)
{
    for (const ImpactEvent &impact : impacts)
        m_sparks.append({QPointF(impact.point.x, impact.point.y), 0.0, impact.intensity,
                         impact.special ? QColor::fromRgb(impact.color) : QColor()});

    // Duel : gerbes d'étincelles de part et d'autre du point de friction.
    if (duel.active) {
        auto *rng = QRandomGenerator::global();
        const Vec2 side = duel.normal.perpendicular();
        for (int k = 0; k < 6; ++k) {
            const double sign = k % 2 == 0 ? 1.0 : -1.0;
            const double spread = (rng->generateDouble() - 0.5) * 1.2;
            const Vec2 dir = (side * sign + duel.normal * spread).normalized();
            const double speed = 0.4 + 0.8 * rng->generateDouble();
            m_embers.append({QPointF(duel.point.x, duel.point.y), QPointF(dir.x, dir.y) * speed, 0.0});
        }
    }
    for (Ember &ember : m_embers) {
        ember.age += dt;
        ember.position += ember.velocity * dt;
        ember.velocity *= 0.92;
    }
    m_embers.erase(std::remove_if(m_embers.begin(), m_embers.end(),
                                  [](const Ember &e) { return e.age > kEmberLifetime; }),
                   m_embers.end());

    if (moving) {
        m_trails.resize(spinners.size());
        for (size_t i = 0; i < spinners.size(); ++i) {
            const Spinner &s = spinners[i];
            if (!s.isActive())
                continue;
            m_trails[i].append(QPointF(s.position.x, s.position.y));
            while (m_trails[i].size() > kTrailLength)
                m_trails[i].removeFirst();
        }
    }

    for (Spark &spark : m_sparks)
        spark.age += dt;
    m_sparks.erase(std::remove_if(m_sparks.begin(), m_sparks.end(),
                                  [](const Spark &s) { return s.age > kSparkLifetime; }),
                   m_sparks.end());
}

void BattleEffects::paintTrails(QPainter &p, const std::vector<Spinner> &spinners,
                                const ToScreen &toScreen, double radius) const
{
    // Traînées lumineuses à la couleur du type.
    for (size_t i = 0; i < m_trails.size() && i < spinners.size(); ++i) {
        const QList<QPointF> &trail = m_trails[i];
        const QColor base = TopPainter::typeColor(spinners[i].top.type());
        for (int k = 1; k < trail.size(); ++k) {
            const double f = double(k) / trail.size();
            p.setPen(QPen(withAlpha(base, 0.12 * f), radius * 1.1 * f, Qt::SolidLine, Qt::RoundCap));
            const QPointF a = toScreen({trail[k - 1].x(), trail[k - 1].y()});
            const QPointF b = toScreen({trail[k].x(), trail[k].y()});
            p.drawLine(a, b);
            p.setPen(QPen(withAlpha(base, 0.45 * f), radius * 0.35 * f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(a, b);
        }
    }
}

void BattleEffects::paintSparks(QPainter &p, const ToScreen &toScreen, double radius,
                                double scale) const
{
    // Étincelles de duel.
    for (const Ember &ember : m_embers) {
        const double t = ember.age / kEmberLifetime;
        const QPointF head = toScreen({ember.position.x(), ember.position.y()});
        const QPointF tail = head - ember.velocity * (scale * 0.04);
        QColor color = (int(ember.age * 100) % 3) ? Theme::kWarning : QColor(Qt::white);
        color.setAlphaF(1.0 - t);
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(tail, head);
    }

    // Étincelles de choc.
    for (const Spark &spark : m_sparks) {
        const double t = spark.age / kSparkLifetime;
        const QPointF c = toScreen({spark.position.x(), spark.position.y()});
        const double size = radius * (0.6 + 1.6 * t) * (0.6 + spark.intensity);
        const QColor color = spark.color.isValid() ? spark.color : Theme::kWarning;
        QRadialGradient flash(c, size);
        flash.setColorAt(0.0, withAlpha(Qt::white, 0.8 * (1.0 - t)));
        flash.setColorAt(1.0, withAlpha(color, 0.0));
        p.setPen(Qt::NoPen);
        p.setBrush(flash);
        p.drawEllipse(c, size, size);
        p.setPen(QPen(withAlpha(color, 1.0 - t), 2.2, Qt::SolidLine, Qt::RoundCap));
        for (int k = 0; k < 8; ++k) {
            const double a = k * M_PI / 4.0 + spark.intensity;
            const QPointF dir(std::cos(a), std::sin(a));
            p.drawLine(c + dir * size * 0.5, c + dir * size * (k % 2 ? 1.0 : 1.35));
        }
    }
}

void BattleEffects::paintStatus(QPainter &p, const Spinner &s, QPointF pos, double radius,
                                double time)
{
    if (!s.isActive())
        return;
    if (s.isSlowed()) {
        // Gelée : coque de glace hexagonale.
        const double r = radius * 1.38;
        QPolygonF shell, inner;
        for (int k = 0; k < 6; ++k) {
            const double a = k * M_PI / 3 + time * 0.3;
            shell << pos + QPointF(std::cos(a), std::sin(a)) * r;
            inner << pos + QPointF(std::cos(a + 0.5), std::sin(a + 0.5)) * r * 0.84;
        }
        p.setBrush(withAlpha(QColor(0x8f, 0xf3, 0xff), 0.18));
        p.setPen(QPen(QColor(255, 255, 255, 210), 1.5));
        p.drawPolygon(shell);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(withAlpha(QColor(0x8f, 0xf3, 0xff), 0.7), 1.0));
        p.drawPolygon(inner);
    }
    if (s.isDisarmed()) {
        // Sonnée : anneau en pointillés et cadenas.
        QPen dashed(Theme::kAccentBright, 1.8);
        dashed.setDashPattern({2, 3});
        p.setPen(dashed);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pos, radius * 1.3, radius * 1.3);
        const QRectF lock(pos.x() + radius * 0.9, pos.y() - radius * 1.6, 18, 18);
        p.fillPath(Theme::chamfer(lock.adjusted(-3, -3, 3, 3), 4), withAlpha(Theme::kField, 0.9));
        Theme::paintIcon(p, QStringLiteral("lock"), lock, Theme::kAccentBright);
    }
}

void BattleEffects::paintProjectiles(QPainter &p, const std::vector<Projectile> &projectiles,
                                     const ToScreen &toScreen, double s, double topRadius,
                                     double stadiumRadius, double time)
{
    for (const Projectile &proj : projectiles) {
        const QColor color = QColor::fromRgb(proj.color);
        const double fade = std::clamp(1.0 - proj.traveled / std::max(1e-6, proj.range), 0.0, 1.0);
        const double alpha = 0.4 + 0.6 * fade;
        if (proj.ring) {
            const QPointF c = toScreen(proj.origin);
            const double r = proj.radius * s;
            QPen limit(withAlpha(color, 0.3), 1.0);
            limit.setDashPattern({2, 6});
            p.setPen(limit);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, proj.range * s, proj.range * s);
            if (proj.move == QLatin1String("vortex")) {
                // Le front de l'anneau part du lanceur et touche au passage…
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(withAlpha(color, 0.25 * alpha), proj.width * s * 3.0));
                p.drawEllipse(c, r, r);
                p.setPen(QPen(withAlpha(color, alpha), 2.0));
                p.drawEllipse(c, r, r);
                // … et le tourbillon, au centre du stadium, y aspire les toupies touchées.
                const QPointF hub = toScreen({0.0, 0.0});
                const double reach = stadiumRadius * s * 0.5;
                for (int k = 0; k < 4; ++k) {
                    QPainterPath arm;
                    for (int i = 0; i <= 36; ++i) {
                        const double a = k * M_PI / 2 + i * 0.14 + time * 3.0;
                        const QPointF q = hub + QPointF(std::cos(a), std::sin(a)) * reach * (1.0 - i / 40.0);
                        i == 0 ? arm.moveTo(q) : arm.lineTo(q);
                    }
                    Theme::strokeGlow(p, arm, withAlpha(color, 0.8 * alpha), 2.2);
                }
                p.setPen(Qt::NoPen);
                for (int k = 0; k < 18; ++k) {
                    const double f = std::fmod(time * 0.9 + k / 18.0, 1.0); // 0 dehors, 1 au centre
                    const double a = k * 2.4 + f * 3.0 + time * 3.0;
                    p.setBrush(withAlpha(Qt::white, alpha * (0.3 + 0.7 * f)));
                    p.drawEllipse(hub + QPointF(std::cos(a), std::sin(a)) * reach * 1.1 * (1.0 - f),
                                  1.5 + 1.5 * f, 1.5 + 1.5 * f);
                }
                p.setBrush(QColor(0, 0, 0, 200));
                p.setPen(QPen(color, 2.0));
                p.drawEllipse(hub, reach * 0.14, reach * 0.14);
            } else if (proj.move == QLatin1String("seisme")) {
                // Failles qui courent jusqu'au front de l'onde.
                for (int k = 0; k < 10; ++k) {
                    double a = k * M_PI / 5 + pseudoRandom(proj.id * 31 + k);
                    double rr = r * 0.15;
                    QPainterPath fault(c + QPointF(std::cos(a), std::sin(a)) * rr);
                    for (int step = 0; rr < r && step < 12; ++step) {
                        rr += r * (0.1 + 0.08 * pseudoRandom(proj.id + k * 7 + step));
                        a += (pseudoRandom(k * 13 + step) - 0.5) * 0.5;
                        fault.lineTo(c + QPointF(std::cos(a), std::sin(a)) * std::min(rr, r));
                    }
                    Theme::strokeGlow(p, fault, withAlpha(color, alpha), 1.8);
                }
                QPen front(withAlpha(color, alpha), 2.0);
                front.setDashPattern({3, 4});
                p.setPen(front);
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(c, r, r);
            } else {
                // Onde de choc : anneaux, décalage chromatique et éclats.
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(withAlpha(color, 0.25 * alpha), proj.width * s * 3.0));
                p.drawEllipse(c, r, r);
                p.setPen(QPen(withAlpha(Theme::kMagenta, 0.5 * alpha), 1.5));
                p.drawEllipse(c, r + 4, r + 4);
                p.setPen(QPen(withAlpha(color, alpha), std::max(2.5, proj.width * s * 0.8)));
                p.drawEllipse(c, r, r);
                p.setPen(QPen(withAlpha(color, 0.45 * alpha), 1.5));
                p.drawEllipse(c, r * 0.7, r * 0.7);
                p.setPen(Qt::NoPen);
                p.setBrush(withAlpha(color, alpha));
                for (int k = 0; k < 14; ++k) {
                    const double a = k * M_PI / 7 + pseudoRandom(proj.id + k) * 0.3;
                    const QPointF dir(std::cos(a), std::sin(a));
                    const QPointF side(-dir.y(), dir.x());
                    const QPointF base = c + dir * (r + 6);
                    p.drawPolygon(QPolygonF{base + side * 3, base + dir * 12, base - side * 3});
                }
            }
            continue;
        }

        // Vague : un front qui file vers sa cible, avec une signature propre.
        const QPointF c = toScreen(proj.position);
        const double angle = qRadiansToDegrees(std::atan2(proj.direction.y, proj.direction.x));
        const double half = (proj.width + topRadius * 0.6) * s;
        p.save();
        p.translate(c);
        p.rotate(angle);
        p.setOpacity(alpha);
        if (proj.move == QLatin1String("lance")) {
            p.setPen(QPen(withAlpha(color, 0.6), 1.3));
            for (int i = 0; i < 7; ++i) {
                const double y = (pseudoRandom(proj.id * 5 + i) - 0.5) * half * 1.6;
                const double x0 = -half * (2.5 + 3.0 * pseudoRandom(i + 3));
                p.drawLine(QPointF(x0, y), QPointF(x0 + half * 1.6, y));
            }
            QPainterPath shaft(QPointF(-half * 4.0, 0));
            shaft.lineTo(QPointF(-half * 0.3, 0));
            Theme::strokeGlow(p, shaft, color, 3.0);
            const QPolygonF head{{half * 0.9, 0}, {-half * 0.45, -half * 0.6}, {-half * 0.15, 0},
                                 {-half * 0.45, half * 0.6}};
            p.setBrush(Qt::white);
            p.setPen(QPen(color, 2.0));
            p.drawPolygon(head);
        } else if (proj.move == QLatin1String("givre")) {
            QLinearGradient cone(QPointF(0, 0), QPointF(-half * 4.0, 0));
            cone.setColorAt(0.0, withAlpha(color, 0.5));
            cone.setColorAt(1.0, withAlpha(color, 0.0));
            p.setPen(Qt::NoPen);
            p.setBrush(cone);
            p.drawPolygon(QPolygonF{{half * 0.2, -half * 0.5}, {half * 0.2, half * 0.5},
                                    {-half * 4.0, half * 1.4}, {-half * 4.0, -half * 1.4}});
            for (int i = 0; i < 8; ++i) {
                const double x = -half * std::fmod(pseudoRandom(proj.id + i) * 4.0 + time * 3.0, 4.0);
                const double y = (pseudoRandom(i * 11 + proj.id) - 0.5) * (half * 0.6 - x * 0.6);
                TopPainter::paintEmblem(p, QStringLiteral("givre"), QPointF(x, y), half * 0.18,
                                        Qt::white, 1.2);
            }
            QPainterPath front(QPointF(-half * 0.2, -half));
            front.quadTo(QPointF(half * 0.6, 0), QPointF(-half * 0.2, half));
            Theme::strokeGlow(p, front, color, 3.0);
        } else if (proj.move == QLatin1String("distorsion")) {
            for (int i = 0; i < 7; ++i) {
                const double jitter = std::sin(time * 37.0 + i * 13.0) * half * 0.5;
                const QColor bar = i % 2 ? Theme::kMagenta : Theme::kCyan;
                p.fillRect(QRectF(-half * 3.2 + jitter, -half + i * half * 0.3, half * 2.8,
                                  half * 0.16),
                           withAlpha(bar, 0.75));
            }
            QPainterPath front(QPointF(-half * 0.3, -half));
            front.quadTo(QPointF(half * 0.5, 0), QPointF(-half * 0.3, half));
            Theme::strokeGlow(p, front, Theme::kMagenta, 3.0);
        } else if (proj.move == QLatin1String("siphon")) {
            for (int k = -1; k <= 1; ++k) {
                QPainterPath tendril;
                for (int i = 0; i <= 24; ++i) {
                    const double x = -half * 4.0 * i / 24.0;
                    const double y = k * half * 0.35 + std::sin(i * 0.6 - time * 10.0 + k) * half * 0.25;
                    i == 0 ? tendril.moveTo(QPointF(0, y)) : tendril.lineTo(QPointF(x, y));
                }
                Theme::strokeGlow(p, tendril, color, k ? 1.2 : 2.6);
            }
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            for (int i = 0; i < 5; ++i) {
                const double x = -half * std::fmod(i * 0.8 + time * 4.0, 4.0);
                QPainterPath drop(QPointF(x, -half * 0.22));
                drop.cubicTo(QPointF(x + half * 0.12, -half * 0.05), QPointF(x + half * 0.14, half * 0.08),
                             QPointF(x, half * 0.14));
                drop.cubicTo(QPointF(x - half * 0.14, half * 0.08), QPointF(x - half * 0.12, -half * 0.05),
                             QPointF(x, -half * 0.22));
                p.drawPath(drop);
            }
        } else {
            for (int k = 4; k >= 0; --k) {
                const double back = k * half * 0.45;
                const double w = half * (1.0 - k * 0.12);
                QPainterPath arc;
                arc.moveTo(-back - w * 0.35, -w);
                arc.quadTo(-back + w * 0.55, 0.0, -back - w * 0.35, w);
                p.setPen(QPen(withAlpha(color, k == 0 ? 0.95 : 0.18), k == 0 ? 4.0 : 6.0,
                              Qt::SolidLine, Qt::RoundCap));
                p.setBrush(Qt::NoBrush);
                p.drawPath(arc);
            }
        }
        p.restore();
    }
}

} // namespace toupie
