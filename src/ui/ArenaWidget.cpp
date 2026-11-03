#include "ui/ArenaWidget.h"

#include "ui/StadiumPainter.h"
#include "ui/TopPainter.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace toupie {

namespace {
constexpr double kHudHeight = 78.0;
constexpr int kTrailLength = 36;
constexpr double kSparkLifetime = 0.4;
constexpr double kAimStep = qDegreesToRadians(3.0);

QString endReasonText(Battle::EndReason reason, int players)
{
    switch (reason) {
    case Battle::EndReason::RingOut:
        return players > 2 ? QObject::tr("Dernière élimination : éjection hors du stadium")
                           : QObject::tr("Éjection hors du stadium");
    case Battle::EndReason::SpinOut:
        return players > 2 ? QObject::tr("Dernière élimination : plus d'endurance")
                           : QObject::tr("Plus d'endurance");
    case Battle::EndReason::Timeout:
        return QObject::tr("Temps écoulé : victoire à l'endurance restante");
    case Battle::EndReason::Forfeit:
        return QObject::tr("Abandon (joueur déconnecté)");
    case Battle::EndReason::None:
        break;
    }
    return {};
}

QString statusText(Spinner::Status status)
{
    switch (status) {
    case Spinner::Status::RingOut:
        return QObject::tr("Éjectée");
    case Spinner::Status::Stopped:
        return QObject::tr("À l'arrêt");
    case Spinner::Status::Forfeit:
        return QObject::tr("Déconnectée");
    default:
        return {};
    }
}
} // namespace

QColor ArenaWidget::slotColor(int slot)
{
    static const QColor colors[] = {QColor(80, 190, 255), QColor(255, 120, 90),
                                    QColor(120, 220, 120), QColor(205, 140, 255)};
    return colors[qBound(0, slot, 3)];
}

ArenaWidget::ArenaWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(520, 560);
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &ArenaWidget::tick);
}

ArenaWidget::~ArenaWidget() = default;

void ArenaWidget::setSession(MatchSession *session)
{
    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);
    m_session = session;
    resetVisuals();
    if (!m_session) {
        m_timer.stop();
        update();
        return;
    }
    connect(m_session, &MatchSession::phaseChanged, this, [this] {
        if (m_session && m_session->state().phase == MatchPhase::Aiming)
            resetVisuals();
    });
    m_clock.restart();
    m_timer.start();
    setFocus();
    update();
}

void ArenaWidget::resetVisuals()
{
    m_trails.clear();
    m_sparks.clear();
    m_flash = 0.0;
    m_lastPhase = MatchPhase::Aiming;
    if (m_session) {
        const MatchState &s = m_session->state();
        m_trails.resize(s.spinners.size());
        m_lastPhase = s.phase;
        // Visée par défaut : vers le centre du stadium.
        const Vec2 start = s.startPositions.value(m_session->localPlayer());
        m_aimAngle = (-start).angle();
    }
}

// ---------------------------------------------------------------------------
// Boucle de jeu
// ---------------------------------------------------------------------------

void ArenaWidget::tick()
{
    if (!m_session)
        return;

    const double dt = std::min(0.05, m_clock.restart() / 1000.0);
    m_session->update(dt);
    const MatchState &state = m_session->state();

    if (state.phase != m_lastPhase) {
        if (state.phase == MatchPhase::Fighting)
            m_flash = 0.8;
        if (state.phase == MatchPhase::Finished)
            emit matchFinished(state.winner);
        m_lastPhase = state.phase;
    }

    for (const ImpactEvent &impact : m_session->takeImpacts())
        m_sparks.append({QPointF(impact.point.x, impact.point.y), 0.0, impact.intensity});

    if (state.phase == MatchPhase::Fighting) {
        m_trails.resize(state.spinners.size());
        for (size_t i = 0; i < state.spinners.size(); ++i) {
            const Spinner &s = state.spinners[i];
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
    m_flash = std::max(0.0, m_flash - dt);

    update();
}

void ArenaWidget::confirmLaunch()
{
    if (m_session && m_session->state().phase == MatchPhase::Aiming
        && !m_session->localHasAimed())
        m_session->submitAim(m_aimAngle);
}

void ArenaWidget::tryDash()
{
    if (m_session && m_session->state().phase == MatchPhase::Fighting)
        m_session->requestDash();
}

// ---------------------------------------------------------------------------
// Entrées
// ---------------------------------------------------------------------------

void ArenaWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_session || m_session->state().phase != MatchPhase::Aiming
        || m_session->localHasAimed())
        return;
    const Vec2 target = toWorld(event->position());
    const Vec2 start = m_session->state().startPositions.value(m_session->localPlayer());
    if ((target - start).length() > 0.02)
        m_aimAngle = (target - start).angle();
}

void ArenaWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    if (!m_session)
        return;
    const MatchPhase phase = m_session->state().phase;
    if (phase == MatchPhase::Aiming && event->button() == Qt::LeftButton) {
        mouseMoveEvent(event);
        confirmLaunch();
    } else if (phase == MatchPhase::Fighting) {
        tryDash();
    }
}

void ArenaWidget::keyPressEvent(QKeyEvent *event)
{
    const MatchPhase phase = m_session ? m_session->state().phase : MatchPhase::Finished;
    const bool aiming = phase == MatchPhase::Aiming && m_session && !m_session->localHasAimed();
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Up:
        if (aiming)
            m_aimAngle -= kAimStep;
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        if (aiming)
            m_aimAngle += kAimStep;
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        confirmLaunch();
        break;
    case Qt::Key_Space:
        if (phase == MatchPhase::Aiming)
            confirmLaunch();
        else
            tryDash();
        break;
    case Qt::Key_R:
        if (phase == MatchPhase::Finished)
            emit rematchRequested();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    update();
}

// ---------------------------------------------------------------------------
// Repères
// ---------------------------------------------------------------------------

double ArenaWidget::scale() const
{
    const double radius = m_session ? m_session->state().geometry.radius : 1.0;
    const double available = std::min<double>(width(), height() - kHudHeight);
    return available / 2.0 * 0.88 / std::max(0.1, radius);
}

QPointF ArenaWidget::center() const
{
    return {width() / 2.0, kHudHeight + (height() - kHudHeight) / 2.0};
}

QPointF ArenaWidget::toScreen(Vec2 world) const
{
    return center() + QPointF(world.x, world.y) * scale();
}

Vec2 ArenaWidget::toWorld(QPointF screen) const
{
    const QPointF local = (screen - center()) / scale();
    return {local.x(), local.y()};
}

// ---------------------------------------------------------------------------
// Rendu
// ---------------------------------------------------------------------------

void ArenaWidget::resizeEvent(QResizeEvent *event)
{
    m_stadiumCacheKey.clear();
    QWidget::resizeEvent(event);
}

void ArenaWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0.0, QColor(24, 26, 38));
    background.setColorAt(1.0, QColor(12, 13, 20));
    p.fillRect(rect(), background);

    if (!m_session)
        return;

    paintStadium(p);
    if (m_session->state().phase == MatchPhase::Aiming)
        paintAim(p);
    paintSpinners(p);
    paintHud(p);
    paintOverlay(p);
}

void ArenaWidget::paintStadium(QPainter &p)
{
    const MatchState &state = m_session->state();
    const QString key = QStringLiteral("%1/%2x%3").arg(state.stadium.id).arg(width()).arg(height());
    const qreal dpr = devicePixelRatioF();
    if (key != m_stadiumCacheKey) {
        m_stadiumCache = QPixmap(size() * dpr);
        m_stadiumCache.setDevicePixelRatio(dpr);
        m_stadiumCache.fill(Qt::transparent);
        QPainter cache(&m_stadiumCache);
        StadiumPainter::paint(cache, state.stadium, state.geometry, center(), scale());
        m_stadiumCacheKey = key;
    }
    p.drawPixmap(0, 0, m_stadiumCache);

    // Points de départ.
    if (state.phase == MatchPhase::Aiming || state.phase == MatchPhase::Countdown) {
        const double r = state.config.topRadius * scale() * 1.5;
        for (int i = 0; i < state.startPositions.size(); ++i) {
            QColor c = slotColor(i);
            c.setAlpha(state.aimed.value(i) ? 200 : 110);
            p.setPen(QPen(c, 2.0, state.aimed.value(i) ? Qt::SolidLine : Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(toScreen(state.startPositions.at(i)), r, r);
        }
    }
}

void ArenaWidget::paintAim(QPainter &p)
{
    if (m_session->localHasAimed())
        return;
    const MatchState &state = m_session->state();
    const int local = m_session->localPlayer();
    const Vec2 start = state.startPositions.value(local);
    const Vec2 dir = Vec2::fromAngle(m_aimAngle);
    const QPointF from = toScreen(start + dir * (state.config.topRadius * 1.3));
    const QPointF to = toScreen(start + dir * 0.4);
    const QColor color = slotColor(local);

    p.setPen(QPen(color, 4.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(from, to);

    const QPointF d = (to - from) / std::max(1e-6, std::hypot((to - from).x(), (to - from).y()));
    const QPointF n(-d.y(), d.x());
    QPolygonF head;
    head << to + d * 14 << to + n * 9 << to - n * 9;
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(head);
}

void ArenaWidget::paintSpinners(QPainter &p)
{
    const MatchState &state = m_session->state();
    const double radius = state.config.topRadius * scale();

    // Traînées.
    for (size_t i = 0; i < m_trails.size() && i < state.spinners.size(); ++i) {
        const QList<QPointF> &trail = m_trails[i];
        const QColor base = TopPainter::typeColor(state.spinners[i].top.type());
        for (int k = 1; k < trail.size(); ++k) {
            QColor c = base;
            c.setAlphaF(0.35 * k / trail.size());
            p.setPen(QPen(c, radius * 0.5 * k / trail.size(), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(toScreen({trail[k - 1].x(), trail[k - 1].y()}),
                       toScreen({trail[k].x(), trail[k].y()}));
        }
    }

    for (int i = 0; i < state.playerCount(); ++i) {
        const Spinner &s = state.spinners[i];
        QPointF pos = toScreen(s.position);

        // Vacillement visuel quand la rotation faiblit.
        if (s.isActive() && s.spinRatio() < state.config.wobbleThreshold) {
            const double amount
                = radius * 0.15 * (1.0 - s.spinRatio() / state.config.wobbleThreshold);
            pos += QPointF(std::cos(s.angle * 3.0), std::sin(s.angle * 3.0)) * amount;
        }

        p.save();
        const bool out = s.status != Spinner::Status::Waiting && !s.isActive();
        if (out)
            p.setOpacity(0.4);
        const double glow = s.isBoosted() ? s.boostTimer / state.config.dashBoostDuration : 0.0;
        TopPainter::paint(p, s.top, pos, radius, s.angle, glow);
        p.restore();

        // Étiquette.
        QFont f = font();
        f.setBold(true);
        p.setFont(f);
        p.setPen(slotColor(i));
        const QString label = i == m_session->localPlayer() ? tr("VOUS") : state.names.value(i);
        p.drawText(QRectF(pos.x() - 70, pos.y() - radius - 22, 140, 18), Qt::AlignCenter,
                   p.fontMetrics().elidedText(label, Qt::ElideRight, 140));
    }

    // Étincelles de choc.
    for (const Spark &spark : m_sparks) {
        const double t = spark.age / kSparkLifetime;
        const QPointF c = toScreen({spark.position.x(), spark.position.y()});
        const double size = radius * (0.6 + 1.6 * t) * (0.6 + spark.intensity);
        QColor color(255, 230, 120);
        color.setAlphaF(1.0 - t);
        p.setPen(QPen(color, 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, size, size);
        for (int k = 0; k < 6; ++k) {
            const double a = k * M_PI / 3.0 + spark.intensity;
            const QPointF dir(std::cos(a), std::sin(a));
            p.drawLine(c + dir * size * 0.6, c + dir * size * 1.2);
        }
    }
}

void ArenaWidget::paintHud(QPainter &p)
{
    const MatchState &state = m_session->state();
    const int count = std::max(1, state.playerCount());
    const double margin = 10.0;
    if (count <= 2) {
        const double panelWidth = std::min(330.0, (width() - 3 * margin) / 2.0 - 40);
        paintSpinnerHud(p, 0, QRectF(margin, 8, panelWidth, kHudHeight - 16));
        if (count == 2)
            paintSpinnerHud(p, 1, QRectF(width() - margin - panelWidth, 8, panelWidth,
                                         kHudHeight - 16));
    } else {
        const double panelWidth = (width() - (count + 1) * margin) / count;
        for (int i = 0; i < count; ++i)
            paintSpinnerHud(p, i, QRectF(margin + i * (panelWidth + margin), 8, panelWidth,
                                         kHudHeight - 16));
    }

    // Chronomètre et stadium.
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 1.6);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(230, 232, 240));
    const double remaining = std::max(0.0, state.config.maxDuration - state.elapsed);
    const QString timer = QString::number(static_cast<int>(std::ceil(remaining)));
    if (count <= 2) {
        p.drawText(QRectF(0, 8, width(), kHudHeight - 16), Qt::AlignHCenter | Qt::AlignVCenter,
                   timer);
    } else {
        p.drawText(QRectF(margin, kHudHeight + 4, 120, 30), Qt::AlignLeft | Qt::AlignVCenter,
                   timer);
    }
    f.setPointSizeF(font().pointSizeF());
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor(170, 175, 190));
    p.drawText(QRectF(width() - 260 - margin, kHudHeight + 4, 260, 24),
               Qt::AlignRight | Qt::AlignVCenter, tr("Stadium %1").arg(state.stadium.name));
}

void ArenaWidget::paintSpinnerHud(QPainter &p, int index, const QRectF &rect)
{
    const MatchState &state = m_session->state();
    const Spinner &s = state.spinners[index];
    const bool local = index == m_session->localPlayer();
    const bool alignRight = state.playerCount() == 2 && index == 1;
    const QColor accent = slotColor(index);
    const Qt::Alignment align = (alignRight ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;

    p.setPen(local ? QPen(accent, 1.5) : QPen(Qt::NoPen));
    p.setBrush(QColor(255, 255, 255, 18));
    p.drawRoundedRect(rect, 8, 8);

    const QRectF inner = rect.adjusted(10, 6, -10, -6);
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(accent);
    const QString title = QStringLiteral("%1 — %2 (%3)")
                              .arg(local ? tr("Vous") : state.names.value(index), s.top.name(),
                                   toString(s.top.type()));
    p.drawText(QRectF(inner.left(), inner.top(), inner.width(), 18), align,
               p.fontMetrics().elidedText(title, Qt::ElideRight, int(inner.width())));

    // Barre d'endurance.
    const QRectF bar(inner.left(), inner.top() + 22, inner.width(), 14);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRoundedRect(bar, 5, 5);
    const double ratio = std::clamp(s.spinRatio(), 0.0, 1.0);
    QRectF fill = bar;
    fill.setWidth(bar.width() * ratio);
    if (alignRight)
        fill.moveRight(bar.right());
    const QColor barColor = ratio > 0.5 ? QColor(70, 200, 110)
                          : ratio > 0.25 ? QColor(235, 185, 50)
                                         : QColor(230, 70, 60);
    p.setBrush(barColor);
    p.drawRoundedRect(fill, 5, 5);
    f.setBold(false);
    f.setPointSizeF(font().pointSizeF() * 0.85);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(bar, Qt::AlignCenter, tr("Endurance %1").arg(qRound(s.spin)));

    // Indicateur de dash ou état.
    const QRectF dashRect(inner.left(), bar.bottom() + 4, inner.width(), 14);
    QString text;
    QColor color;
    const QString status = statusText(s.status);
    if (!status.isEmpty()) {
        text = status;
        color = QColor(230, 110, 100);
    } else if (state.phase == MatchPhase::Aiming) {
        text = state.aimed.value(index) ? tr("Prêt à lancer") : tr("Vise…");
        color = state.aimed.value(index) ? QColor(120, 230, 150) : QColor(170, 170, 180);
    } else if (s.isBoosted()) {
        text = tr("DASH !");
        color = QColor(255, 220, 90);
    } else if (s.dashCooldown > 0.0) {
        text = tr("Dash dans %1 s").arg(s.dashCooldown, 0, 'f', 1);
        color = QColor(170, 170, 180);
    } else if (s.spin <= state.config.dashCost) {
        text = tr("Dash impossible");
        color = QColor(170, 90, 90);
    } else {
        text = local ? tr("Dash prêt (Espace / clic)") : tr("Dash prêt");
        color = QColor(120, 230, 150);
    }
    p.setPen(color);
    p.drawText(dashRect, align, p.fontMetrics().elidedText(text, Qt::ElideRight,
                                                            int(dashRect.width())));
}

void ArenaWidget::paintOverlay(QPainter &p)
{
    const MatchState &state = m_session->state();
    const QRectF area(0, kHudHeight, width(), height() - kHudHeight);
    QFont big = font();
    big.setBold(true);

    auto banner = [&](const QString &text, double sizeFactor, const QColor &color, double dy) {
        big.setPointSizeF(font().pointSizeF() * sizeFactor);
        p.setFont(big);
        const QRectF r = area.translated(0, dy);
        p.setPen(QColor(0, 0, 0, 170));
        p.drawText(r.translated(2, 2), Qt::AlignCenter, text);
        p.setPen(color);
        p.drawText(r, Qt::AlignCenter, text);
    };

    switch (state.phase) {
    case MatchPhase::Aiming: {
        QString hint;
        if (m_session->localHasAimed()) {
            const int ready = static_cast<int>(std::count(state.aimed.begin(), state.aimed.end(), true));
            hint = tr("En attente des autres joueurs (%1/%2)").arg(ready).arg(state.playerCount());
        } else {
            hint = tr("Visez : souris ou ← →   ·   Lancer : clic ou Entrée");
        }
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(font().pointSizeF() * 1.15);
        p.setFont(f);
        const QRectF box(width() / 2.0 - 230, area.top() + 4, 460, 30);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 150));
        p.drawRoundedRect(box, 8, 8);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, hint);
        break;
    }
    case MatchPhase::Countdown: {
        const int n = std::max(1, static_cast<int>(std::ceil(state.countdown)));
        banner(QString::number(n), 5.0, QColor(255, 255, 255), 0);
        break;
    }
    case MatchPhase::Fighting:
        if (m_flash > 0.0) {
            p.save();
            p.setOpacity(std::min(1.0, m_flash * 2.0));
            banner(tr("Partez !"), 4.0, QColor(255, 220, 90), 0);
            p.restore();
        }
        break;
    case MatchPhase::Finished: {
        p.fillRect(area, QColor(0, 0, 0, 120));
        const int winner = state.winner;
        const int local = m_session->localPlayer();
        const QString title = winner == local ? tr("Victoire !")
                            : winner < 0      ? tr("Match nul")
                                              : tr("Défaite…");
        const QColor color = winner == local ? QColor(120, 230, 150)
                           : winner < 0      ? QColor(230, 230, 230)
                                             : QColor(240, 110, 90);
        banner(title, 3.6, color, -70);
        if (winner >= 0 && winner != local)
            banner(tr("%1 remporte le combat").arg(state.names.value(winner)), 1.3,
                   slotColor(winner), -20);
        banner(endReasonText(state.endReason, state.playerCount()), 1.1, QColor(235, 235, 240),
               14);

        if (state.playerCount() > 2) {
            QStringList lines;
            for (int rank = 0; rank < state.ranking.size(); ++rank) {
                const int idx = state.ranking.at(rank);
                lines << QStringLiteral("%1. %2 (%3)")
                             .arg(rank + 1)
                             .arg(state.names.value(idx), state.spinners[idx].top.name());
            }
            banner(lines.join('\n'), 1.0, QColor(220, 222, 230), 40 + 12 * lines.size());
        }
        break;
    }
    }
}

} // namespace toupie
