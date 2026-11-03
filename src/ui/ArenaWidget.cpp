#include "ui/ArenaWidget.h"

#include "ui/TopPainter.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace toupie {

namespace {
constexpr double kHudHeight = 78.0;
constexpr double kCountdownSeconds = 3.0;
constexpr int kTrailLength = 36;
constexpr double kSparkLifetime = 0.4;
constexpr double kAimStep = qDegreesToRadians(3.0);

const QColor kPlayerColor(80, 190, 255);
const QColor kBotColor(255, 120, 90);

QString endReasonText(Battle::EndReason reason)
{
    switch (reason) {
    case Battle::EndReason::RingOut:
        return QObject::tr("Éjection hors du stadium");
    case Battle::EndReason::SpinOut:
        return QObject::tr("Plus d'endurance");
    case Battle::EndReason::Timeout:
        return QObject::tr("Temps écoulé : victoire à l'endurance restante");
    case Battle::EndReason::None:
        break;
    }
    return {};
}
} // namespace

ArenaWidget::ArenaWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(480, 520);
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &ArenaWidget::tick);
}

ArenaWidget::~ArenaWidget() = default;

void ArenaWidget::startMatch(const Top &player, const Top &opponent, Difficulty difficulty)
{
    m_player = player;
    m_opponent = opponent;
    m_difficulty = difficulty;
    restart();
}

void ArenaWidget::restart()
{
    auto *rng = QRandomGenerator::global();
    m_battle = std::make_unique<Battle>(m_player, m_opponent, BattleConfig{}, rng->generate());
    m_bot = std::make_unique<BotController>(1, m_difficulty, rng->generate());
    m_botAngle = m_bot->chooseLaunchAngle(*m_battle);
    m_aimAngle = 0.0; // Vers le centre du stadium.
    m_countdown = 0.0;
    m_flash = 0.0;
    for (auto &trail : m_trails)
        trail.clear();
    m_sparks.clear();

    setPhase(Phase::Aiming);
    m_clock.restart();
    m_timer.start();
    setFocus();
    update();
}

void ArenaWidget::stop()
{
    m_timer.stop();
    setPhase(Phase::Idle);
}

void ArenaWidget::setPhase(Phase phase)
{
    if (m_phase == phase)
        return;
    m_phase = phase;
    emit phaseChanged(phase);
}

// ---------------------------------------------------------------------------
// Boucle de jeu
// ---------------------------------------------------------------------------

void ArenaWidget::tick()
{
    const double dt = std::min(0.05, m_clock.restart() / 1000.0);

    if (m_phase == Phase::Countdown) {
        m_countdown -= dt;
        if (m_countdown <= 0.0) {
            m_battle->launch(m_aimAngle, m_botAngle);
            m_flash = 0.8;
            setPhase(Phase::Fighting);
        }
    } else if (m_phase == Phase::Fighting && m_battle) {
        m_bot->update(*m_battle, dt);
        m_battle->advance(dt);

        const QList<ImpactEvent> impacts = m_battle->takeImpacts();
        for (const ImpactEvent &impact : impacts)
            m_sparks.append({QPointF(impact.point.x, impact.point.y), 0.0, impact.intensity});

        for (int i = 0; i < 2; ++i) {
            const Vec2 pos = m_battle->spinner(i).position;
            m_trails[i].append(QPointF(pos.x, pos.y));
            while (m_trails[i].size() > kTrailLength)
                m_trails[i].removeFirst();
        }

        if (m_battle->isFinished()) {
            setPhase(Phase::Finished);
            emit matchFinished(m_battle->winner());
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
    if (m_phase != Phase::Aiming)
        return;
    m_countdown = kCountdownSeconds;
    setPhase(Phase::Countdown);
}

void ArenaWidget::tryDash()
{
    if (m_phase == Phase::Fighting && m_battle)
        m_battle->requestDash(0);
}

// ---------------------------------------------------------------------------
// Entrées
// ---------------------------------------------------------------------------

void ArenaWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_phase != Phase::Aiming || !m_battle)
        return;
    const Vec2 target = toWorld(event->position());
    const Vec2 start = m_battle->startPosition(0);
    if ((target - start).length() > 0.02)
        m_aimAngle = (target - start).angle();
}

void ArenaWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    if (m_phase == Phase::Aiming && event->button() == Qt::LeftButton) {
        mouseMoveEvent(event);
        confirmLaunch();
    } else if (m_phase == Phase::Fighting) {
        tryDash();
    }
}

void ArenaWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Up:
        if (m_phase == Phase::Aiming)
            m_aimAngle -= kAimStep;
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        if (m_phase == Phase::Aiming)
            m_aimAngle += kAimStep;
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        confirmLaunch();
        break;
    case Qt::Key_Space:
        if (m_phase == Phase::Aiming)
            confirmLaunch();
        else
            tryDash();
        break;
    case Qt::Key_R:
        if (m_phase == Phase::Finished)
            restart();
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
    const double radius = m_battle ? m_battle->config().stadiumRadius : 1.0;
    const double available = std::min<double>(width(), height() - kHudHeight);
    return available / 2.0 * 0.9 / radius;
}

QPointF ArenaWidget::toScreen(Vec2 world) const
{
    const QPointF center(width() / 2.0, kHudHeight + (height() - kHudHeight) / 2.0);
    return center + QPointF(world.x, world.y) * scale();
}

Vec2 ArenaWidget::toWorld(QPointF screen) const
{
    const QPointF center(width() / 2.0, kHudHeight + (height() - kHudHeight) / 2.0);
    const QPointF local = (screen - center) / scale();
    return {local.x(), local.y()};
}

// ---------------------------------------------------------------------------
// Rendu
// ---------------------------------------------------------------------------

void ArenaWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0.0, QColor(24, 26, 38));
    background.setColorAt(1.0, QColor(12, 13, 20));
    p.fillRect(rect(), background);

    if (!m_battle)
        return;

    paintStadium(p);
    if (m_phase == Phase::Aiming)
        paintAim(p);
    paintSpinners(p);
    paintHud(p);
    paintOverlay(p);
}

void ArenaWidget::paintStadium(QPainter &p)
{
    const BattleConfig &cfg = m_battle->config();
    const QPointF center = toScreen({0, 0});
    const double r = cfg.stadiumRadius * scale();

    // Rebord.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(70, 74, 92));
    p.drawEllipse(center, r * 1.05, r * 1.05);

    // Cuvette : plus claire au centre.
    QRadialGradient bowl(center, r);
    bowl.setColorAt(0.0, QColor(215, 220, 230));
    bowl.setColorAt(0.75, QColor(160, 168, 185));
    bowl.setColorAt(1.0, QColor(105, 112, 132));
    p.setBrush(bowl);
    p.drawEllipse(center, r, r);

    // Cercles concentriques.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 60), 1.0));
    for (double f : {0.33, 0.66})
        p.drawEllipse(center, r * f, r * f);
    p.setPen(QPen(QColor(40, 44, 60), 2.0));
    p.drawEllipse(center, r, r);

    // Points de départ.
    if (m_phase == Phase::Aiming || m_phase == Phase::Countdown) {
        for (int i = 0; i < 2; ++i) {
            QColor c = i == 0 ? kPlayerColor : kBotColor;
            c.setAlpha(110);
            p.setPen(QPen(c, 2.0, Qt::DashLine));
            p.drawEllipse(toScreen(m_battle->startPosition(i)), cfg.topRadius * scale() * 1.5,
                          cfg.topRadius * scale() * 1.5);
        }
    }
}

void ArenaWidget::paintAim(QPainter &p)
{
    const BattleConfig &cfg = m_battle->config();
    const Vec2 start = m_battle->startPosition(0);
    const Vec2 dir = Vec2::fromAngle(m_aimAngle);
    const QPointF from = toScreen(start + dir * (cfg.topRadius * 1.3));
    const QPointF to = toScreen(start + dir * 0.4);

    p.setPen(QPen(kPlayerColor, 4.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(from, to);

    const QPointF d = (to - from) / std::hypot((to - from).x(), (to - from).y());
    const QPointF n(-d.y(), d.x());
    QPolygonF head;
    head << to + d * 14 << to + n * 9 << to - n * 9;
    p.setPen(Qt::NoPen);
    p.setBrush(kPlayerColor);
    p.drawPolygon(head);
}

void ArenaWidget::paintSpinners(QPainter &p)
{
    const BattleConfig &cfg = m_battle->config();
    const double radius = cfg.topRadius * scale();

    // Traînées.
    for (int i = 0; i < 2; ++i) {
        const QList<QPointF> &trail = m_trails[i];
        const QColor base = TopPainter::typeColor(m_battle->spinner(i).top.type());
        for (int k = 1; k < trail.size(); ++k) {
            QColor c = base;
            c.setAlphaF(0.35 * k / trail.size());
            p.setPen(QPen(c, radius * 0.5 * k / trail.size(), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(toScreen({trail[k - 1].x(), trail[k - 1].y()}),
                       toScreen({trail[k].x(), trail[k].y()}));
        }
    }

    for (int i = 0; i < 2; ++i) {
        const Spinner &s = m_battle->spinner(i);
        QPointF pos = toScreen(s.position);

        // Vacillement visuel quand la rotation faiblit.
        if (s.isActive() && s.spinRatio() < cfg.wobbleThreshold) {
            const double amount = radius * 0.15 * (1.0 - s.spinRatio() / cfg.wobbleThreshold);
            pos += QPointF(std::cos(s.angle * 3.0), std::sin(s.angle * 3.0)) * amount;
        }

        p.save();
        if (!s.isActive() && s.status != Spinner::Status::Waiting)
            p.setOpacity(0.45);
        const double glow = s.isBoosted() ? s.boostTimer / cfg.dashBoostDuration : 0.0;
        TopPainter::paint(p, s.top, pos, radius, s.angle, glow);
        p.restore();

        // Étiquette.
        const QColor label = i == 0 ? kPlayerColor : kBotColor;
        QFont f = font();
        f.setBold(true);
        p.setFont(f);
        p.setPen(label);
        p.drawText(QRectF(pos.x() - 40, pos.y() - radius - 22, 80, 18), Qt::AlignCenter,
                   i == 0 ? tr("VOUS") : tr("BOT"));
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
    const double margin = 12.0;
    const double panelWidth = std::min(330.0, (width() - 3 * margin) / 2.0 - 40);
    paintSpinnerHud(p, m_battle->spinner(0), QRectF(margin, 8, panelWidth, kHudHeight - 16),
                    true, false);
    paintSpinnerHud(p, m_battle->spinner(1),
                    QRectF(width() - margin - panelWidth, 8, panelWidth, kHudHeight - 16), false,
                    true);

    // Chronomètre.
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 1.6);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(230, 232, 240));
    const double remaining
        = std::max(0.0, m_battle->config().maxDuration - m_battle->elapsed());
    p.drawText(QRectF(0, 8, width(), kHudHeight - 16), Qt::AlignHCenter | Qt::AlignVCenter,
               QString::number(static_cast<int>(std::ceil(remaining))));
}

void ArenaWidget::paintSpinnerHud(QPainter &p, const Spinner &s, const QRectF &rect, bool player,
                                  bool alignRight)
{
    const BattleConfig &cfg = m_battle->config();
    const QColor accent = player ? kPlayerColor : kBotColor;
    const Qt::Alignment align = (alignRight ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 18));
    p.drawRoundedRect(rect, 8, 8);

    const QRectF inner = rect.adjusted(10, 6, -10, -6);
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(accent);
    const QString title = QStringLiteral("%1 — %2 (%3)")
                              .arg(player ? tr("Vous") : tr("Bot %1").arg(toString(m_difficulty)),
                                   s.top.name(), toString(s.top.type()));
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

    // Indicateur de dash.
    const QRectF dashRect(inner.left(), bar.bottom() + 4, inner.width(), 14);
    QString dashText;
    QColor dashColor;
    if (s.isBoosted()) {
        dashText = tr("DASH !");
        dashColor = QColor(255, 220, 90);
    } else if (s.dashCooldown > 0.0) {
        dashText = tr("Dash dans %1 s").arg(s.dashCooldown, 0, 'f', 1);
        dashColor = QColor(170, 170, 180);
    } else if (s.spin <= cfg.dashCost) {
        dashText = tr("Dash impossible");
        dashColor = QColor(170, 90, 90);
    } else {
        dashText = player ? tr("Dash prêt (Espace / clic)") : tr("Dash prêt");
        dashColor = QColor(120, 230, 150);
    }
    p.setPen(dashColor);
    p.drawText(dashRect, align, dashText);
}

void ArenaWidget::paintOverlay(QPainter &p)
{
    const QRectF area(0, kHudHeight, width(), height() - kHudHeight);
    QFont big = font();
    big.setBold(true);

    auto banner = [&](const QString &text, double sizeFactor, const QColor &color, double dy) {
        big.setPointSizeF(font().pointSizeF() * sizeFactor);
        p.setFont(big);
        const QRectF r = area.translated(0, dy);
        p.setPen(QColor(0, 0, 0, 160));
        p.drawText(r.translated(2, 2), Qt::AlignCenter, text);
        p.setPen(color);
        p.drawText(r, Qt::AlignCenter, text);
    };

    switch (m_phase) {
    case Phase::Aiming:
        banner(tr("Visez avec la souris ou ← →\nClic ou Entrée pour lancer"), 1.2,
               QColor(30, 34, 48), -area.height() * 0.2);
        break;
    case Phase::Countdown: {
        const int n = static_cast<int>(std::ceil(m_countdown));
        banner(QString::number(n), 5.0, QColor(255, 255, 255), 0);
        break;
    }
    case Phase::Fighting:
        if (m_flash > 0.0) {
            p.save();
            p.setOpacity(std::min(1.0, m_flash * 2.0));
            banner(tr("Partez !"), 4.0, QColor(255, 220, 90), 0);
            p.restore();
        }
        break;
    case Phase::Finished: {
        p.fillRect(area, QColor(0, 0, 0, 110));
        const int winner = m_battle->winner();
        const QString title = winner == 0 ? tr("Victoire !")
                            : winner == 1 ? tr("Défaite…")
                                          : tr("Match nul");
        const QColor color = winner == 0 ? QColor(120, 230, 150)
                           : winner == 1 ? QColor(240, 110, 90)
                                         : QColor(230, 230, 230);
        banner(title, 3.6, color, -40);
        banner(endReasonText(m_battle->endReason()), 1.3, QColor(235, 235, 240), 30);
        banner(tr("R : revanche"), 1.0, QColor(200, 200, 210), 70);
        break;
    }
    case Phase::Idle:
        break;
    }
}

} // namespace toupie
