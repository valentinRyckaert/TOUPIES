#include "ui/ArenaWidget.h"

#include "game/SpecialMove.h"
#include "ui/Controls.h"
#include "ui/TutorialSession.h"
#include "ui/StadiumPainter.h"
#include "ui/Theme.h"
#include "ui/TopPainter.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace toupie {

namespace {
constexpr double kTopBar = 56.0;      ///< Bandeau du haut : chrono, manche, stadium.
constexpr double kBottomHud = 140.0; ///< Fiches des toupies en bas.
constexpr double kAimStep = qDegreesToRadians(3.0);
constexpr double kPowerStep = 0.05;
constexpr double kMinArrow = 0.14; ///< Longueur de flèche (monde) à la force minimale.
constexpr double kMaxArrow = 0.6;  ///< … et à la force maximale.

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
    static const QColor colors[] = {Theme::kAccent, Theme::kDanger, Theme::kCyan, Theme::kWarning};
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
    m_effects.reset();
    m_sounds.reset();
    m_duelResultTimer = 0.0;
    m_flash = 0.0;
    m_lastPhase = MatchPhase::Aiming;
    if (m_session) {
        const MatchState &s = m_session->state();
        m_lastPhase = s.phase;
        // Visée par défaut : vers le centre du stadium.
        const Vec2 start = s.startPositions.value(m_session->localPlayer());
        m_aimAngle = (-start).angle();
        m_aimPower = 1.0;
        m_steerKeys.clear();
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

    const QList<ImpactEvent> impacts = m_session->takeImpacts();
    m_effects.update(dt, impacts, state.spinners, state.duel, state.phase == MatchPhase::Fighting);
    const int local = m_session->localPlayer();
    // Musique de combat jusqu'à la fin de la partie ; écran de fin sans musique.
    Audio &audio = Audio::instance();
    audio.setMusic(state.phase == MatchPhase::Finished ? Audio::Music::None : Audio::Music::Battle);
    for (const SoundCue &cue : m_sounds.update(state, impacts, local))
        audio.play(cue);
    for (const ImpactEvent &impact : impacts) {
        if (impact.duelLoser == ImpactEvent::kNoDuel)
            continue;
        const bool involved = impact.first == local || impact.second == local;
        if (impact.duelLoser < 0) {
            m_duelResult = tr("Duel nul !");
            m_duelResultColor = QColor(230, 230, 235);
        } else if (involved) {
            const bool lost = impact.duelLoser == local;
            m_duelResult = lost ? tr("Duel perdu…") : tr("Duel gagné !");
            m_duelResultColor = lost ? QColor(240, 110, 90) : Theme::kAccent;
        } else {
            m_duelResult = tr("%1 perd le duel").arg(state.names.value(impact.duelLoser));
            m_duelResultColor = slotColor(impact.duelLoser);
        }
        m_duelResultTimer = 1.6;
    }
    m_duelResultTimer = std::max(0.0, m_duelResultTimer - dt);
    m_flash = std::max(0.0, m_flash - dt);

    update();
}

void ArenaWidget::confirmLaunch()
{
    if (m_session && m_session->state().phase == MatchPhase::Aiming
        && !m_session->localHasAimed())
        m_session->submitAim(m_aimAngle, m_aimPower);
}

void ArenaWidget::trySpecial()
{
    if (m_session && m_session->state().phase == MatchPhase::Fighting)
        m_session->requestSpecial();
    setFocus();
}

double ArenaWidget::powerForLength(double length) const
{
    if (!m_session)
        return 1.0;
    const BattleConfig &cfg = m_session->state().config;
    const double t = std::clamp((length - kMinArrow) / (kMaxArrow - kMinArrow), 0.0, 1.0);
    return cfg.launchPowerMin + t * (cfg.launchPowerMax - cfg.launchPowerMin);
}

double ArenaWidget::lengthForPower(double power) const
{
    if (!m_session)
        return (kMinArrow + kMaxArrow) / 2.0;
    const BattleConfig &cfg = m_session->state().config;
    const double t = std::clamp((power - cfg.launchPowerMin)
                                    / std::max(1e-6, cfg.launchPowerMax - cfg.launchPowerMin),
                                0.0, 1.0);
    return kMinArrow + t * (kMaxArrow - kMinArrow);
}

void ArenaWidget::updateSteering()
{
    if (!m_session)
        return;
    const Controls &controls = Controls::instance();
    Vec2 steer;
    for (int code : std::as_const(m_steerKeys)) {
        if (controls.triggers(code, Action::SteerLeft))
            steer.x -= 1.0;
        if (controls.triggers(code, Action::SteerRight))
            steer.x += 1.0;
        if (controls.triggers(code, Action::SteerUp))
            steer.y -= 1.0;
        if (controls.triggers(code, Action::SteerDown))
            steer.y += 1.0;
    }
    m_session->setSteering(steer.normalized());
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
    const double length = (target - start).length();
    if (length > 0.02) {
        m_aimAngle = (target - start).angle();
        // Plus le curseur est loin, plus le lancer est puissant.
        m_aimPower = powerForLength(length);
    }
}

void ArenaWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    if (event->button() == Qt::LeftButton && m_specialRect.contains(event->position())) {
        trySpecial(); // La carte du coup spécial, pas un dash.
        return;
    }
    const int code = Controls::mouseCode(event->button());
    const Controls &controls = Controls::instance();
    // Lancer à la souris : on vise d'abord là où se trouve le curseur.
    if (m_session && m_session->state().phase == MatchPhase::Aiming
        && (controls.triggers(code, Action::Dash) || controls.triggers(code, Action::Launch)))
        mouseMoveEvent(event);
    pressInput(code, false);
}

void ArenaWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_steerKeys.remove(Controls::mouseCode(event->button())))
        updateSteering();
}

bool ArenaWidget::pressInput(int code, bool autoRepeat)
{
    QList<Action> actions = Controls::instance().actionsFor(code);
    if (actions.isEmpty())
        return false;
    // Tutoriel : seules les actions de l'étape en cours passent.
    if (auto *tutorial = qobject_cast<TutorialSession *>(m_session.data())) {
        actions.removeIf([tutorial](Action a) { return !tutorial->handleAction(a); });
        if (actions.isEmpty())
            return true;
    }
    const MatchPhase phase = m_session ? m_session->state().phase : MatchPhase::Finished;
    const bool aiming = phase == MatchPhase::Aiming && m_session && !m_session->localHasAimed();
    const bool steering = actions.contains(Action::SteerLeft) || actions.contains(Action::SteerRight)
                       || actions.contains(Action::SteerUp) || actions.contains(Action::SteerDown);
    if (steering && !aiming) {
        // Pendant le combat : pilotage tant que l'entrée est maintenue.
        if (!autoRepeat) {
            m_steerKeys.insert(code);
            updateSteering();
        }
        return true;
    }
    const BattleConfig *cfg = m_session ? &m_session->state().config : nullptr;
    for (Action action : actions) {
        switch (action) {
        case Action::SteerLeft:
            m_aimAngle -= kAimStep;
            break;
        case Action::SteerRight:
            m_aimAngle += kAimStep;
            break;
        case Action::SteerUp:
            if (cfg)
                m_aimPower = std::min(cfg->launchPowerMax, m_aimPower + kPowerStep);
            break;
        case Action::SteerDown:
            if (cfg)
                m_aimPower = std::max(cfg->launchPowerMin, m_aimPower - kPowerStep);
            break;
        case Action::Special:
            trySpecial();
            break;
        case Action::Launch:
            confirmLaunch();
            break;
        case Action::Dash:
            if (phase == MatchPhase::Aiming)
                confirmLaunch();
            else if (!autoRepeat) // Une touche maintenue ne martèle pas une rafale.
                tryDash();
            break;
        case Action::Rematch:
            if (phase == MatchPhase::Finished)
                emit rematchRequested();
            break;
        }
    }
    update();
    return true;
}

void ArenaWidget::keyPressEvent(QKeyEvent *event)
{
    if (!pressInput(event->key(), event->isAutoRepeat()))
        QWidget::keyPressEvent(event);
}

void ArenaWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat() && m_steerKeys.remove(event->key())) {
        updateSteering();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void ArenaWidget::focusOutEvent(QFocusEvent *event)
{
    // Touches relâchées hors de la fenêtre : on arrête de piloter.
    if (!m_steerKeys.isEmpty()) {
        m_steerKeys.clear();
        updateSteering();
    }
    QWidget::focusOutEvent(event);
}

// ---------------------------------------------------------------------------
// Repères
// ---------------------------------------------------------------------------

double ArenaWidget::unitScale(const QSizeF &arena)
{
    // Une seule échelle pour tous les stadiums (de même surface) : le plus large tient
    // dans la vue, et les autres gardent à l'écran la taille qu'ils ont en jeu.
    static const double largest = [] {
        double size = 1.0;
        for (const Stadium &stadium : Stadium::all())
            size = std::max(size, stadium.size);
        return size;
    }();
    const double available = std::min(arena.width(), arena.height() - kTopBar - kBottomHud);
    return available / 2.0 * 0.95 / largest;
}

double ArenaWidget::scale() const
{
    double base = 1.0; // Rayon du stadium classique dans ces règles.
    if (m_session) {
        const MatchState &state = m_session->state();
        base = state.config.stadiumRadius / std::max(0.1, state.stadium.size);
    }
    return unitScale(size()) / std::max(0.1, base);
}

QPointF ArenaWidget::center() const
{
    return {width() / 2.0, kTopBar + (height() - kTopBar - kBottomHud) / 2.0};
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

namespace {

QColor withAlpha(QColor c, double a)
{
    c.setAlphaF(std::clamp(a, 0.0, 1.0));
    return c;
}

/// Texte à décalage chromatique (magenta / cyan), comme le logo.
void chromaText(QPainter &p, const QRectF &r, int flags, const QString &text, const QFont &font,
                const QColor &color, double offset)
{
    p.setFont(font);
    if (offset > 0.0) { // 0 : texte net, sans les reflets magenta / cyan.
        p.setPen(withAlpha(Theme::kMagenta, 0.75));
        p.drawText(r.translated(-offset, 0), flags, text);
        p.setPen(withAlpha(Theme::kCyan, 0.75));
        p.drawText(r.translated(offset, offset * 0.4), flags, text);
    }
    p.setPen(color);
    p.drawText(r, flags, text);
}

/// Bandeau incliné : plein (raison de fin) ou en fondu (« Partez ! », duel).
void band(QPainter &p, const QRectF &r, const QColor &color, bool solid)
{
    if (solid) {
        QLinearGradient g(r.topLeft(), r.topRight());
        g.setColorAt(0.0, color);
        g.setColorAt(1.0, color.lighter(125));
        p.fillPath(Theme::skewed(r, r.height() * 0.35), g);
        return;
    }
    QLinearGradient g(r.topLeft(), r.topRight());
    g.setColorAt(0.0, withAlpha(color, 0.0));
    g.setColorAt(0.2, withAlpha(color, 0.28));
    g.setColorAt(0.8, withAlpha(color, 0.28));
    g.setColorAt(1.0, withAlpha(color, 0.0));
    p.fillPath(Theme::skewed(r, r.height() * 0.35), g);
    p.setPen(QPen(color, 2.0));
    p.drawLine(QPointF(r.left() + r.width() * 0.1, r.top()), QPointF(r.right(), r.top()));
    p.drawLine(QPointF(r.left(), r.bottom()), QPointF(r.right() - r.width() * 0.1, r.bottom()));
}

} // namespace

void ArenaWidget::resizeEvent(QResizeEvent *event)
{
    m_stadiumCacheKey.clear();
    QWidget::resizeEvent(event);
}

void ArenaWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!m_session) {
        Theme::paintBackground(p, rect());
        return;
    }

    paintStadium(p);
    if (m_session->state().phase == MatchPhase::Aiming)
        paintAim(p);
    paintSpinners(p);
    BattleEffects::paintProjectiles(p, m_session->state().projectiles,
                                    [this](Vec2 v) { return toScreen(v); }, scale(),
                                    m_session->state().config.topRadius,
                                    m_session->state().geometry.radius, m_session->state().elapsed);
    paintHud(p);
    paintDuel(p);
    paintOverlay(p);
    paintTutorial(p);
}

void ArenaWidget::paintStadium(QPainter &p)
{
    const MatchState &state = m_session->state();
    const QString key = QStringLiteral("%1/%2x%3").arg(state.stadium.id).arg(width()).arg(height());
    const qreal dpr = devicePixelRatioF();
    if (key != m_stadiumCacheKey) {
        // Fond et stadium ne changent qu'avec la taille : une seule image.
        m_stadiumCache = QPixmap(size() * dpr);
        m_stadiumCache.setDevicePixelRatio(dpr);
        QPainter cache(&m_stadiumCache);
        Theme::paintBackground(cache, rect());
        StadiumPainter::paint(cache, state.stadium, state.geometry, center(), scale());
        m_stadiumCacheKey = key;
    }
    p.drawPixmap(0, 0, m_stadiumCache);

    // Points de départ : losange et cercle à la couleur de chaque place.
    if (state.phase == MatchPhase::Aiming || state.phase == MatchPhase::Countdown) {
        const double r = state.config.topRadius * scale() * 1.5;
        for (int i = 0; i < state.startPositions.size(); ++i) {
            const QPointF c = toScreen(state.startPositions.at(i));
            const bool aimed = state.aimed.value(i);
            QPen ring(withAlpha(slotColor(i), aimed ? 0.9 : 0.5), 2.0);
            if (!aimed)
                ring.setDashPattern({4, 4});
            p.setPen(ring);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, r, r);
            const double d = r * 1.25;
            p.setPen(QPen(withAlpha(slotColor(i), 0.7), 1.5));
            p.drawPolygon(QPolygonF{c + QPointF(0, -d), c + QPointF(d, 0), c + QPointF(0, d),
                                    c + QPointF(-d, 0)});
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
    const double length = lengthForPower(m_aimPower);
    const QPointF from = toScreen(start + dir * (state.config.topRadius * 1.3));
    const QPointF to = toScreen(start + dir * length);
    const QColor color = slotColor(local);

    // Plus la flèche est longue et épaisse, plus le lancer est puissant.
    const double t = (m_aimPower - state.config.launchPowerMin)
                   / std::max(1e-6, state.config.launchPowerMax - state.config.launchPowerMin);
    QPainterPath shaft(from);
    shaft.lineTo(to);
    Theme::strokeGlow(p, shaft, color, 3.0 + 4.0 * t);

    const QPointF d = (to - from) / std::max(1e-6, std::hypot((to - from).x(), (to - from).y()));
    const QPointF n(-d.y(), d.x());
    const double size = 14.0 + 6.0 * t;
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(QPolygonF{to + d * size, to + n * size * 0.65 - d * size * 0.2, to,
                            to - n * size * 0.65 - d * size * 0.2});

    // Libellé au-delà de la pointe.
    const QPointF labelCenter = to + d * (size + 40);
    const QRectF labelRect(labelCenter.x() - 62, labelCenter.y() - 14, 124, 28);
    p.fillPath(Theme::chamfer(labelRect, 8), withAlpha(Theme::kField, 0.92));
    p.setPen(QPen(color, 1.3));
    p.setBrush(Qt::NoBrush);
    p.drawPath(Theme::chamfer(labelRect, 8));
    QFont f = Theme::displayFont(14);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(f);
    p.setPen(color);
    p.drawText(labelRect, Qt::AlignCenter, tr("Force %1 %").arg(qRound(m_aimPower * 100)).toUpper());
}

void ArenaWidget::paintSpinners(QPainter &p)
{
    const MatchState &state = m_session->state();
    const double radius = state.config.topRadius * scale();

    const auto screen = [this](Vec2 v) { return toScreen(v); };
    m_effects.paintTrails(p, state.spinners, screen, radius);

    for (int i = 0; i < state.playerCount(); ++i) {
        const Spinner &s = state.spinners[i];
        QPointF pos = toScreen(s.position);

        // Duel : les toupies tremblent sur place.
        if (state.duel.sideOf(i) >= 0) {
            const double t = state.elapsed * 60.0 + i * 1.7;
            pos += QPointF(std::sin(t * 1.3), std::cos(t * 1.7)) * (radius * 0.06);
        }
        // Vacillement visuel quand la rotation faiblit.
        if (s.isActive() && s.spinRatio() < state.config.wobbleThreshold) {
            const double amount
                = radius * 0.15 * (1.0 - s.spinRatio() / state.config.wobbleThreshold);
            pos += QPointF(std::cos(s.angle * 3.0), std::sin(s.angle * 3.0)) * amount;
        }

        p.save();
        double drawRadius = radius;
        const bool out = s.status != Spinner::Status::Waiting && !s.isActive();
        if (out)
            p.setOpacity(0.4);
        if (s.status == Spinner::Status::RingOut) {
            // Éjectée : affichée tombée au-delà du bord, plus petite.
            const Vec2 normal = state.geometry.outwardNormal(s.position);
            pos = toScreen(s.position + normal * (state.config.topRadius * 1.6));
            drawRadius = radius * 0.75;
            p.setOpacity(0.35);
        }
        const double glow = s.isBoosted() ? s.boostTimer / state.config.dashBoostDuration : 0.0;
        TopPainter::paint(p, s.top, pos, drawRadius, s.angle, glow);
        BattleEffects::paintStatus(p, s, pos, drawRadius, state.elapsed);
        p.restore();

        // Étiquette inclinée à la couleur de la place.
        const QString label = i == m_session->localPlayer() ? tr("VOUS") : state.names.value(i);
        QFont f = Theme::displayFont(12);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        const QString text = QFontMetrics(f).elidedText(label.toUpper(), Qt::ElideRight, 130);
        const double tw = QFontMetrics(f).horizontalAdvance(text) + 26;
        const QRectF tag(pos.x() - tw / 2.0, pos.y() - drawRadius - 34, tw, 20);
        p.save();
        if (out)
            p.setOpacity(0.5);
        p.fillPath(Theme::skewed(tag, 7), slotColor(i));
        p.setPen(Qt::NoPen);
        p.setBrush(slotColor(i));
        p.drawPolygon(QPolygonF{QPointF(pos.x() - 5, tag.bottom()), QPointF(pos.x() + 5, tag.bottom()),
                                QPointF(pos.x(), tag.bottom() + 6)});
        p.setFont(f);
        p.setPen(QColor(0x05, 0x04, 0x0f));
        p.drawText(tag, Qt::AlignCenter, text);
        p.restore();
    }

    m_effects.paintSparks(p, screen, radius, scale());
}

void ArenaWidget::setCaption(const QString &caption)
{
    m_caption = caption;
    update();
}

void ArenaWidget::paintHud(QPainter &p)
{
    const MatchState &state = m_session->state();
    const int count = std::max(1, state.playerCount());
    const double margin = 12.0;
    auto spaced = [](QFont f, double spacing) {
        f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
        return f;
    };

    // En haut à droite : stadium, nom de code et limite de temps (à gauche : boutons de la page).
    const QColor accent = QColor::fromRgb(state.stadium.accent);
    p.setFont(spaced(Theme::displayFont(18), 2.0));
    p.setPen(Theme::kText);
    p.drawText(QRectF(width() - margin - 420, 8, 420, 24), Qt::AlignRight | Qt::AlignVCenter,
               state.stadium.name.toUpper());
    p.setFont(Theme::bodyFont(15, QFont::Bold));
    p.setPen(accent);
    p.drawText(QRectF(width() - margin - 420, 32, 420, 20), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1 · %2").arg(state.stadium.codename.toUpper(),
                                             state.config.maxDuration > 0.0
                                                 ? tr("Limite %1 s").arg(qRound(state.config.maxDuration))
                                                 : tr("Sans limite de temps").toLower()));

    // Onglet central : manche et score (ou chrono).
    const QRectF tab(width() / 2.0 - 160, 2, 320, kTopBar - 6);
    p.fillPath(Theme::chamfer(tab, 0, 0, 16, 16), withAlpha(Theme::kPanel, 0.92));
    p.setPen(QPen(Theme::kBorderBright, 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(Theme::chamfer(tab, 0, 0, 16, 16));
    QString heading = state.series ? tr("Manche %1").arg(state.series->rounds + 1)
                    : count > 2    ? tr("Mêlée à %1").arg(count)
                                   : tr("Combat");
    if (state.config.maxDuration > 0.0) {
        heading += QStringLiteral(" · %1 s").arg(static_cast<int>(
            std::ceil(std::max(0.0, state.config.maxDuration - state.elapsed))));
    }
    p.setFont(spaced(Theme::displayFont(13), 2.5));
    p.setPen(Theme::kMuted);
    p.drawText(QRectF(tab.left(), tab.top() + 3, tab.width(), 18), Qt::AlignCenter, heading.toUpper());
    p.setFont(Theme::displayFont(24));
    const QRectF line(tab.left(), tab.top() + 20, tab.width(), 28);
    if (state.series) {
        p.setPen(slotColor(0));
        p.drawText(line.adjusted(0, 0, -tab.width() / 2 - 30, 0), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(state.series->score[0]));
        p.setPen(Theme::kDim);
        p.drawText(line, Qt::AlignCenter, QStringLiteral("–"));
        p.setPen(slotColor(1));
        p.drawText(line.adjusted(tab.width() / 2 + 30, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(state.series->score[1]));
    } else {
        const int seconds = static_cast<int>(state.elapsed);
        p.setPen(Theme::kText);
        p.drawText(line, Qt::AlignCenter,
                   QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0')));
    }
    if (!m_caption.isEmpty()) { // En haut à droite, sous le stadium : hors de l'arène.
        p.setFont(Theme::bodyFont(15, QFont::DemiBold));
        p.setPen(Theme::kMuted);
        p.drawText(QRectF(width() - margin - 420, 54, 420, 20), Qt::AlignRight | Qt::AlignVCenter, m_caption);
    }

    // Fiches des toupies en bas.
    const double cardHeight = kBottomHud - 12;
    const double top = height() - kBottomHud + 4;
    const int local = m_session->localPlayer();
    const bool special = state.config.specialsEnabled && local >= 0 && local < count;
    QRectF localCard;
    if (count <= 2) {
        const double cardWidth = std::min(600.0, (width() - 3 * margin) / 2.0);
        const QRectF left(margin, top, cardWidth, cardHeight);
        paintSpinnerHud(p, 0, left, false);
        const QRectF right(width() - margin - cardWidth, top, cardWidth, cardHeight);
        if (count == 2)
            paintSpinnerHud(p, 1, right, true);
        localCard = local == 1 ? right : left;
    } else {
        const double cardWidth = (width() - (count + 1) * margin) / count;
        for (int i = 0; i < count; ++i) {
            const QRectF card(margin + i * (cardWidth + margin), top, cardWidth, cardHeight);
            paintSpinnerHud(p, i, card, false);
            if (i == local)
                localCard = card;
        }
    }
    // Coup spécial en haut à gauche, sous les boutons de la page : dans ce coin, ni le
    // stadium, ni l'aide de visée, ni les jauges de duel ne passent.
    m_specialRect = special ? QRectF(margin, kTopBar + 48, std::min(440.0, width() / 2.0 - 160 - 2 * margin), 64)
                            : QRectF();
    if (special)
        paintSpecial(p, m_specialRect);

    // « DASH ! » au-dessus de la fiche du joueur pendant l'élan.
    if (local >= 0 && local < count && (state.spinners[local].isDashing() || state.spinners[local].isBoosted())
        && !localCard.isNull()) {
        const double w = std::min(270.0, localCard.width());
        const bool right = count == 2 && local == 1;
        const QRectF box(right ? localCard.right() - w : localCard.left(), localCard.top() - 48, w, 36);
        const QPainterPath path = Theme::skewed(box, 12);
        p.fillPath(path, withAlpha(Theme::kCyan, 0.14));
        p.setPen(QPen(Theme::kCyan, 1.3));
        p.drawPath(path);
        Theme::paintIcon(p, QStringLiteral("dash"), QRectF(box.left() + 24, box.center().y() - 9, 18, 18), Theme::kCyan);
        p.setFont(spaced(Theme::displayFont(14), 2.0));
        p.setPen(Theme::kCyan);
        p.drawText(box.adjusted(54, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, tr("DASH !"));
    }
}

void ArenaWidget::paintSpecial(QPainter &p, const QRectF &r)
{
    const MatchState &state = m_session->state();
    const Spinner &s = state.spinners[m_session->localPlayer()];
    const auto move = SpecialMove::find(s.top.special());
    if (!move)
        return;
    const bool available = state.phase == MatchPhase::Fighting && !s.specialUsed && s.isActive() && !s.isDisarmed();
    p.save();
    p.setOpacity(available ? 1.0 : s.specialUsed ? 0.4 : 0.7);
    const QPainterPath card = Theme::chamfer(r, 16);
    QLinearGradient fill(r.topLeft(), r.topRight());
    fill.setColorAt(0.0, QColor(0x15, 0x14, 0x52));
    fill.setColorAt(0.55, withAlpha(Theme::kViolet, 0.85));
    fill.setColorAt(1.0, Theme::kMagenta);
    p.fillPath(card, fill);
    if (available) { // Pulsation : le coup est prêt.
        p.setPen(QPen(withAlpha(Theme::kMagenta, 0.35 + 0.25 * std::sin(state.elapsed * 5.0)), 6.0));
        p.drawPath(card);
    }
    p.setPen(QPen(QColor(0xc9, 0xb8, 0xff), 1.3));
    p.drawPath(card);
    const QRectF icon(r.left() + 14, r.top() + 12, r.height() - 24, r.height() - 24);
    p.fillPath(Theme::chamfer(icon, 8), withAlpha(Theme::kVoid, 0.85));
    p.setPen(QPen(QColor(255, 255, 255, 170), 1.0));
    p.drawPath(Theme::chamfer(icon, 8));
    TopPainter::paintEmblem(p, s.top.special(), icon.center(), icon.height() * 0.32, Qt::white, 1.8);

    const QString key = Controls::codeText(Controls::instance().binding(Action::Special, 0));
    QFont capFont = Theme::displayFont(12);
    const double capWidth = std::max(26, QFontMetrics(capFont).horizontalAdvance(key) + 14);
    const QRectF cap(r.right() - 24 - capWidth, r.center().y() - 12, capWidth, 24);
    const QRectF text(icon.right() + 16, r.top(), cap.left() - icon.right() - 28, r.height());
    QFont small = Theme::displayFont(11);
    small.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    p.setFont(small);
    p.setPen(QColor(255, 255, 255, 190));
    const QString caption = s.specialUsed ? tr("Coup spécial · utilisé") : tr("Coup spécial · coûte ¼ de l'endurance");
    p.drawText(QRectF(text.left(), r.top() + r.height() * 0.15, text.width(), 18), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(small).elidedText(caption.toUpper(), Qt::ElideRight, int(text.width())));
    QFont name = Theme::displayFont(20);
    name.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(name);
    p.setPen(Qt::white);
    p.drawText(QRectF(text.left(), r.top() + r.height() * 0.4, text.width(), 30), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(name).elidedText(move->name.toUpper(), Qt::ElideRight, int(text.width())));
    p.fillPath(Theme::chamfer(cap, 5, 0, 5, 0), withAlpha(Theme::kField, 0.9));
    p.setPen(QPen(Theme::kBorderBright, 1.0));
    p.drawPath(Theme::chamfer(cap, 5, 0, 5, 0));
    p.setFont(capFont);
    p.setPen(Theme::kCyan);
    p.drawText(cap, Qt::AlignCenter, key);
    p.restore();
}

void ArenaWidget::paintSpinnerHud(QPainter &p, int index, const QRectF &rect, bool mirrored)
{
    const MatchState &state = m_session->state();
    const Spinner &s = state.spinners[index];
    const bool local = index == m_session->localPlayer();
    const QColor accent = slotColor(index);
    const bool out = s.status != Spinner::Status::Waiting && !s.isActive();
    // Miroir (fiche de droite) : toupie à droite, endurance à gauche.
    const Qt::Alignment near = mirrored ? Qt::AlignRight : Qt::AlignLeft;
    const Qt::Alignment far = mirrored ? Qt::AlignLeft : Qt::AlignRight;

    p.save();
    if (out)
        p.setOpacity(0.55);
    const QPainterPath card = mirrored ? Theme::chamfer(rect, 0, 22, 0, 22) : Theme::chamfer(rect, 22, 0, 22, 0);
    p.fillPath(card, withAlpha(QColor(0x0b, 0x09, 0x30), 0.9));
    p.setPen(QPen(local ? accent : Theme::kBorder, local ? 2.0 : 1.4));
    p.setBrush(Qt::NoBrush);
    p.drawPath(card);

    const double mini = std::min(32.0, rect.height() * 0.26);
    const double iconX = mirrored ? rect.right() - 20 - mini : rect.left() + 20 + mini;
    TopPainter::paint(p, s.top, QPointF(iconX, rect.top() + 14 + mini), mini, s.angle);
    const double inset = 40 + mini * 2;
    const QRectF texts = rect.adjusted(mirrored ? 110 : inset, 0, mirrored ? -inset : -110, 0);
    QFont title = Theme::displayFont(20);
    title.setLetterSpacing(QFont::AbsoluteSpacing, 2.5);
    p.setFont(title);
    p.setPen(accent);
    const QString who = (local ? tr("Vous") : state.names.value(index)).toUpper();
    p.drawText(QRectF(texts.left(), rect.top() + 12, texts.width(), 28), near | Qt::AlignVCenter,
               QFontMetrics(title).elidedText(who, Qt::ElideRight, int(texts.width())));
    p.setFont(Theme::bodyFont(16, QFont::DemiBold));
    p.setPen(Theme::kMuted);
    p.drawText(QRectF(texts.left(), rect.top() + 40, texts.width(), 20), near | Qt::AlignVCenter,
               p.fontMetrics().elidedText(QStringLiteral("%1 · %2").arg(s.top.name(), toString(s.top.type())),
                                          Qt::ElideRight, int(texts.width())));

    // Endurance : valeur et jauge inclinée (repère rouge à 25 %).
    const double ratio = std::clamp(s.spinRatio(), 0.0, 1.0);
    const bool low = ratio < 0.25;
    const QColor bar = low ? Theme::kDanger : ratio < 0.5 ? Theme::kWarning : Theme::kAccent;
    p.setFont(Theme::displayFont(34));
    p.setPen(low ? Theme::kDanger : Theme::kText);
    p.drawText(QRectF(rect.left() + 22, rect.top() + 8, rect.width() - 44, 44), far | Qt::AlignVCenter,
               QString::number(qRound(s.spin)));
    const QRectF gauge(rect.left() + 20, rect.top() + rect.height() * 0.6, rect.width() - 40, 16);
    p.fillPath(Theme::skewed(gauge, 9), Theme::kField);
    p.setPen(QPen(Theme::kBorder, 1.0));
    p.drawPath(Theme::skewed(gauge, 9));
    if (ratio > 0.0) {
        QRectF fill = gauge;
        fill.setWidth(std::max(10.0, gauge.width() * ratio));
        QLinearGradient g(fill.topLeft(), fill.topRight());
        g.setColorAt(0.0, bar);
        g.setColorAt(1.0, low ? Theme::kWarning : s.isSlowed() ? Theme::kCyan : Theme::kAccentBright);
        if (low && s.isActive())
            p.setOpacity(p.opacity() * (0.65 + 0.35 * std::sin(state.elapsed * 25.0)));
        p.fillPath(Theme::skewed(fill, 9), g);
        p.fillPath(Theme::skewed(QRectF(fill.left() + 2, fill.top() + 2, fill.width() - 4, 5), 3),
                   QColor(255, 255, 255, 70));
        p.setOpacity(out ? 0.55 : 1.0);
    }
    const double mark = gauge.left() + gauge.width() * 0.25;
    p.setPen(QPen(Theme::kDanger, 2.0));
    p.drawLine(QPointF(mark + 4, gauge.top() - 3), QPointF(mark - 4, gauge.bottom() + 3));

    // « ENDURANCE » puis l'état : dash pour le joueur (son coup a sa carte), coup spécial sinon.
    const QRectF row(rect.left() + 20, gauge.bottom() + 6, rect.width() - 40, rect.bottom() - gauge.bottom() - 10);
    QFont label = Theme::bodyFont(13, QFont::Bold);
    label.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    p.setFont(label);
    p.setPen(Theme::kMuted);
    p.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, tr("Endurance").toUpper());

    QString text;
    QColor color;
    const auto move = SpecialMove::find(s.top.special());
    const QString status = statusText(s.status);
    if (!status.isEmpty()) {
        text = status;
        color = Theme::kDanger;
    } else if (state.duel.sideOf(index) >= 0) {
        text = tr("DUEL !");
        color = Theme::kWarning;
    } else if (s.isSlowed()) {
        text = tr("Gelée (%1 s)").arg(s.slowTimer, 0, 'f', 1);
        color = QColor(0x8f, 0xf3, 0xff);
    } else if (s.isDisarmed()) {
        text = tr("Sonnée : ni dash ni coup spécial (%1 s)").arg(s.disarmTimer, 0, 'f', 1);
        color = Theme::kAccentBright;
    } else if (state.phase == MatchPhase::Aiming) {
        text = state.aimed.value(index) ? tr("Prêt à lancer") : tr("Vise…");
        color = state.aimed.value(index) ? Theme::kAccent : Theme::kMuted;
    } else if (!local && move && state.config.specialsEnabled) {
        text = s.specialUsed ? tr("%1 — utilisé").arg(move->name) : tr("%1 disponible").arg(move->name);
        color = s.specialUsed ? Theme::kDim : QColor::fromRgb(move->color);
    } else if (s.isBoosted()) {
        text = tr("DASH !");
        color = Theme::kCyan;
    } else if (s.dashCooldown > 0.0) {
        text = tr("Dash dans %1 s").arg(s.dashCooldown, 0, 'f', 1);
        color = Theme::kMuted;
    } else if (s.spin <= state.config.dashCost) {
        text = tr("Dash impossible");
        color = Theme::kDanger;
    } else {
        text = local ? tr("Dash prêt (%1)").arg(Controls::instance().text(Action::Dash)) : tr("Dash prêt");
        color = Theme::kAccent;
    }
    QFont f = Theme::bodyFont(15, QFont::Bold);
    p.setFont(f);
    p.setPen(color);
    const double width = row.width() - QFontMetrics(label).horizontalAdvance(tr("Endurance").toUpper()) - 16;
    p.drawText(row, Qt::AlignRight | Qt::AlignVCenter, QFontMetrics(f).elidedText(text, Qt::ElideRight, int(width)));
    p.restore();
}

void ArenaWidget::paintOverlay(QPainter &p)
{
    const MatchState &state = m_session->state();
    const QRectF area(0, kTopBar, width(), height() - kTopBar - kBottomHud);
    const QPointF c = area.center();

    switch (state.phase) {
    case MatchPhase::Aiming: {
        QString hint;
        if (m_session->localHasAimed()) {
            const int ready = static_cast<int>(std::count(state.aimed.begin(), state.aimed.end(), true));
            hint = tr("En attente des autres joueurs (%1/%2)").arg(ready).arg(state.playerCount());
        } else {
            const Controls &controls = Controls::instance();
            hint = tr("Direction : souris ou %1 %2  ·  Force : distance ou %3 %4  ·  Lancer : %5")
                       .arg(Controls::codeText(controls.binding(Action::SteerLeft, 0)),
                            Controls::codeText(controls.binding(Action::SteerRight, 0)),
                            Controls::codeText(controls.binding(Action::SteerUp, 0)),
                            Controls::codeText(controls.binding(Action::SteerDown, 0)),
                            controls.text(Action::Dash));
        }
        const QFont f = Theme::bodyFont(16, QFont::Bold);
        const double w = std::min(width() - 40.0, QFontMetrics(f).horizontalAdvance(hint) + 60.0);
        const QRectF box(width() / 2.0 - w / 2.0, area.top() + 6, w, 34);
        p.fillPath(Theme::chamfer(box, 10), withAlpha(Theme::kPanel, 0.92));
        p.setPen(QPen(Theme::kCyan, 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawPath(Theme::chamfer(box, 10));
        p.setFont(f);
        p.setPen(Theme::kText);
        p.drawText(box, Qt::AlignCenter, QFontMetrics(f).elidedText(hint, Qt::ElideRight, int(w - 20)));
        break;
    }
    case MatchPhase::Countdown: {
        const int n = std::max(1, static_cast<int>(std::ceil(state.countdown)));
        chromaText(p, area, Qt::AlignCenter, QString::number(n), Theme::displayFont(150, true),
                   Qt::white, 7);
        break;
    }
    case MatchPhase::Fighting:
        if (m_flash > 0.0) {
            p.save();
            p.setOpacity(std::min(1.0, m_flash * 2.0));
            const QRectF r(c.x() - 330, c.y() - 55, 660, 110);
            band(p, r, Theme::kAccent, false);
            chromaText(p, r, Qt::AlignCenter, tr("Partez !").toUpper(), Theme::displayFont(64, true),
                       Qt::white, 5);
            p.restore();
        }
        if (m_duelResultTimer > 0.0) {
            p.save();
            p.setOpacity(std::min(1.0, m_duelResultTimer * 1.5));
            const QRectF r(c.x() - 300, c.y() - 170, 600, 70);
            band(p, r, m_duelResultColor, false);
            chromaText(p, r, Qt::AlignCenter, m_duelResult.toUpper(), Theme::displayFont(38, true),
                       m_duelResultColor, 3);
            p.restore();
        }
        break;
    case MatchPhase::Finished: {
        p.fillRect(area, QColor(5, 4, 15, 150));
        const int winner = state.winner;
        const int local = m_session->localPlayer();
        const QString title = winner == local ? tr("Victoire !")
                            : winner < 0      ? tr("Match nul")
                                              : tr("Défaite…");
        const QColor color = winner == local ? Theme::kAccent
                           : winner < 0      ? Theme::kText
                                             : Theme::kDanger;
        double y = c.y() - 150;
        if (state.playerCount() > 2)
            y -= 40;
        // Texte net, sans reflets magenta / cyan (illisibles sur un titre de cette taille).
        chromaText(p, QRectF(0, y, width(), 110), Qt::AlignCenter, title.toUpper(),
                   Theme::displayFont(std::min(96.0, width() / 9.0), true), color, 0);
        y += 112;
        const QString reason = endReasonText(state.endReason, state.playerCount());
        if (!reason.isEmpty()) {
            QFont f = Theme::displayFont(15);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
            const double w = QFontMetrics(f).horizontalAdvance(reason.toUpper()) + 80;
            const QRectF r(c.x() - w / 2.0, y, w, 38);
            band(p, r, color, true);
            p.setFont(f);
            p.setPen(winner < 0 ? Theme::kVoid : QColor(0x03, 0x14, 0x0a));
            p.drawText(r, Qt::AlignCenter, reason.toUpper());
            y += 54;
        }
        auto line = [&](const QString &text, const QFont &font, const QColor &col) {
            p.setFont(font);
            p.setPen(col);
            p.drawText(QRectF(0, y, width(), 30), Qt::AlignCenter, text);
            y += 32;
        };
        if (winner >= 0 && winner != local)
            line(tr("%1 remporte le combat").arg(state.names.value(winner)),
                 Theme::displayFont(20), slotColor(winner));
        if (state.series) {
            MatchSeries after = *state.series;
            after.recordRound(winner, state.endReason);
            const int gained = winner >= 0 ? after.score[winner] - state.series->score[winner] : 0;
            QString text = tr("Score du match : %1 – %2").arg(after.score[0]).arg(after.score[1]);
            if (gained > 0)
                text += tr("  (+%n pt(s) pour %1)", nullptr, gained)
                            .arg(winner == local ? tr("vous") : state.names.value(winner));
            line(text, Theme::displayFont(18), Theme::kAccent);
            if (after.isOver()) {
                const bool won = after.winner() == local;
                line(won ? tr("Match remporté !") : tr("Match perdu"), Theme::displayFont(26, true),
                     won ? Theme::kAccent : Theme::kDanger);
            } else {
                line(tr("Manche suivante dans quelques secondes…"), Theme::bodyFont(17), Theme::kMuted);
            }
        }

        if (state.playerCount() > 2) {
            // Classement de la mêlée.
            const double w = std::min(440.0, width() - 40.0);
            const QRectF panel(c.x() - w / 2.0, y + 4, w, 18 + 30.0 * state.ranking.size());
            Theme::paintPanel(p, panel, Theme::kCyan, 14);
            for (int rank = 0; rank < state.ranking.size(); ++rank) {
                const int idx = state.ranking.at(rank);
                const QRectF row(panel.left() + 22, panel.top() + 10 + rank * 30, w - 44, 28);
                p.setFont(Theme::displayFont(18));
                p.setPen(rank == 0 ? Theme::kWarning : Theme::kMuted);
                p.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("%1.").arg(rank + 1));
                p.setPen(slotColor(idx));
                p.drawText(row.adjusted(40, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                           (idx == local ? tr("Vous") : state.names.value(idx)).toUpper());
                p.setFont(Theme::bodyFont(15, QFont::DemiBold));
                p.setPen(Theme::kMuted);
                p.drawText(row, Qt::AlignRight | Qt::AlignVCenter, state.spinners[idx].top.name());
            }
        }
        break;
    }
    }
}

void ArenaWidget::paintTutorial(QPainter &p)
{
    auto *tutorial = qobject_cast<TutorialSession *>(m_session.data());
    const MatchState &state = m_session->state();
    if (!tutorial || tutorial->bubble().isEmpty() || state.phase == MatchPhase::Countdown)
        return;

    // Bulle au-dessus de la toupie désignée (ou au centre), avec une pointe.
    const QFont f = Theme::bodyFont(17, QFont::DemiBold);
    p.setFont(f);
    const double bubbleWidth = std::min(440.0, width() - 40.0);
    const QString text = tutorial->bubble();
    const QRectF textRect = p.fontMetrics().boundingRect(
        QRect(0, 0, int(bubbleWidth - 40), 1000), Qt::TextWordWrap, text);
    const double footer = tutorial->isPaused() ? 24.0 : 0.0;
    const QSizeF size(bubbleWidth, textRect.height() + 30 + footer);
    const int target = tutorial->bubbleTarget();
    // Sans toupie désignée (dernière bulle) : sous le bandeau de victoire.
    QPointF anchor = center() + QPointF(0, height() * 0.2);
    if (target >= 0 && target < state.playerCount()) {
        const Spinner &s = state.spinners[target];
        anchor = toScreen(s.status == Spinner::Status::Waiting ? state.startPositions.value(target)
                                                               : s.position);
    }
    const double radius = state.config.topRadius * scale();
    const bool below = target >= 0 && anchor.y() - radius - 40 - size.height() < kTopBar;
    QRectF box(anchor.x() - size.width() / 2.0,
               below ? anchor.y() + radius + 18 : anchor.y() - radius - 40 - size.height(),
               size.width(), size.height());
    if (target < 0)
        box.moveCenter(anchor);
    box.moveLeft(std::clamp(box.left(), 10.0, width() - box.width() - 10.0));

    QPainterPath path = Theme::chamfer(box, 16);
    if (target >= 0) {
        const double x = std::clamp(anchor.x(), box.left() + 30, box.right() - 30);
        const double edge = below ? box.top() : box.bottom();
        const double tip = below ? anchor.y() + radius + 4 : anchor.y() - radius - 4;
        QPolygonF tail;
        tail << QPointF(x - 11, edge) << QPointF(x, tip) << QPointF(x + 11, edge);
        path.addPolygon(tail);
        path.setFillRule(Qt::WindingFill);
        path = path.simplified();
    }
    p.fillPath(path, withAlpha(QColor(0x0b, 0x09, 0x30), 0.96));
    p.setPen(QPen(Theme::kCyan, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.fillRect(QRectF(box.left(), box.top() + 18, 4, 36), Theme::kCyan);
    p.setFont(f);
    p.setPen(Theme::kText);
    p.drawText(box.adjusted(20, 15, -20, -15 - footer), Qt::TextWordWrap, text);
    if (tutorial->isPaused()) {
        QFont small = Theme::displayFont(12);
        small.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        p.setFont(small);
        p.setPen(Theme::kAccent);
        p.drawText(box.adjusted(20, 0, -20, -12), Qt::AlignBottom | Qt::AlignRight,
                   tr("Jeu en pause").toUpper());
    }
}

void ArenaWidget::paintDuel(QPainter &p)
{
    const MatchState &state = m_session->state();
    if (!state.duel.active || state.phase != MatchPhase::Fighting)
        return;
    const Duel &duel = state.duel;
    const int local = m_session->localPlayer();
    const int mine = duel.sideOf(local);
    const double big = std::min(640.0, width() - 120.0);
    const double small = big * 0.6;
    const double bottom = height() - kBottomHud - 18;

    if (state.config.duelMode == DuelMode::Rush) {
        paintRush(p, QRectF(width() / 2.0 - big / 2.0, bottom - 34, big, 30));
        return;
    }
    if (mine >= 0) {
        // Ma barre en bas, celle de l'adversaire juste au-dessus en plus petit.
        const QRectF mineRect(width() / 2.0 - big / 2.0, bottom - 34, big, 34);
        const QRectF otherRect(width() / 2.0 - small / 2.0, mineRect.top() - 66, small, 16);
        const int other = mine == 0 ? duel.second : duel.first;
        paintDuelBar(p, otherRect, other, duel.sides[1 - mine], false);
        paintDuelBar(p, mineRect, local, duel.sides[mine], true);
    } else {
        for (int k = 0; k < 2; ++k) {
            const QRectF rect(width() / 2.0 - small / 2.0, bottom - 16 - (1 - k) * 52, small, 16);
            paintDuelBar(p, rect, k == 0 ? duel.first : duel.second, duel.sides[k], false);
        }
    }
}

void ArenaWidget::paintRush(QPainter &p, const QRectF &rect)
{
    // Tir à la corde : chaque camp à un bout de la barre (le joueur à gauche) ; le
    // curseur part du milieu et file vers le camp qui se fait distancer.
    const MatchState &state = m_session->state();
    const Duel &duel = state.duel;
    const int mine = duel.sideOf(m_session->localPlayer());
    const int left = mine >= 0 ? mine : 0;
    const int spinners[2] = {left == 0 ? duel.first : duel.second, left == 0 ? duel.second : duel.first};
    const double half = rect.width() / 2.0, slant = rect.height() * 0.5;
    const double lead = (left == 0 ? duel.rope() : -duel.rope()) / double(std::max(1, state.config.rushLimit));
    const double remaining = std::max(0.0, state.config.rushDuration - duel.elapsed);

    QFont title = Theme::displayFont(mine >= 0 ? 15 : 12);
    title.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    const QString text = mine >= 0 ? tr("Rafale ! Martelez %1").arg(Controls::instance().text(Action::Dash))
                                   : tr("Rafale");
    const QString line = QStringLiteral("%1  ·  %2 s").arg(text.toUpper()).arg(remaining, 0, 'f', 1);
    const double labelWidth = QFontMetrics(title).horizontalAdvance(line) + 40;
    const QRectF label(rect.center().x() - labelWidth / 2.0, rect.top() - 40, labelWidth, 24);
    p.fillPath(Theme::chamfer(label, 6), withAlpha(Theme::kField, 0.9));
    p.setFont(title);
    p.setPen(Theme::kWarning);
    p.drawText(label, Qt::AlignCenter, line);

    // Deux moitiés aux couleurs des camps, repère central, curseur.
    const QPainterPath bar = Theme::skewed(rect, slant);
    for (int k = 0; k < 2; ++k) {
        const QColor c = slotColor(spinners[k]);
        QLinearGradient g(rect.topLeft(), rect.topRight());
        g.setColorAt(k == 0 ? 0.0 : 1.0, withAlpha(c, 0.85));
        g.setColorAt(0.5, withAlpha(c, 0.2));
        p.save();
        p.setClipRect(QRectF(k == 0 ? rect.left() - slant : rect.center().x(), rect.top(), half + slant, rect.height()));
        p.fillPath(bar, g);
        p.restore();
    }
    p.setPen(QPen(QColor(255, 255, 255, 120), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(bar);
    p.setPen(QPen(Qt::white, 2.0));
    p.drawLine(QPointF(rect.center().x(), rect.top() - 4), QPointF(rect.center().x(), rect.bottom() + 4));
    const double x = rect.center().x() + std::clamp(lead, -1.0, 1.0) * half;
    p.fillRect(QRectF(x - 10, rect.top() - 12, 20, rect.height() + 24), QColor(255, 255, 255, 50));
    p.setPen(QPen(QColor(0x05, 0x04, 0x0f), 1.5));
    p.setBrush(Theme::kWarning);
    p.drawPolygon(QPolygonF{QPointF(x - 9, rect.top() - 12), QPointF(x + 9, rect.top() - 12),
                            QPointF(x + 4, rect.bottom() + 12), QPointF(x - 4, rect.bottom() + 12)});

    // Noms et appuis aux deux bouts, temps restant au-dessus de la barre.
    p.setFont(Theme::displayFont(13));
    for (int k = 0; k < 2; ++k) {
        const int side = k == 0 ? left : 1 - left;
        const QString who = spinners[k] == m_session->localPlayer() ? tr("Vous") : state.names.value(spinners[k]);
        p.setPen(slotColor(spinners[k]));
        p.drawText(QRectF(k == 0 ? rect.left() : rect.center().x(), rect.bottom() + 8, half, 18),
                   (k == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
                   QStringLiteral("%1 · %2").arg(who.toUpper()).arg(duel.sides[side].presses));
    }
    const double progress = remaining / std::max(0.1, state.config.rushDuration);
    p.fillRect(QRectF(rect.center().x() - half * progress, rect.top() - 13, rect.width() * progress, 3),
               Theme::kWarning);
}

void ArenaWidget::paintDuelBar(QPainter &p, const QRectF &rect, int spinner, const DuelSide &side,
                               bool local)
{
    const MatchState &state = m_session->state();
    const double half = rect.width() / 2.0;
    const double slant = rect.height() * 0.5;

    // Libellé au-dessus de la barre.
    QFont f = Theme::displayFont(local ? 15 : 12);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    QString text = local ? tr("DUEL ! %1 quand le curseur est dans le vert")
                               .arg(Controls::instance().text(Action::Dash))
                         : tr("Duel — %1").arg(state.names.value(spinner));
    if (side.done)
        text = local ? tr("Série réussie ! En attente de l'adversaire…")
                     : tr("%1 : série réussie").arg(state.names.value(spinner));
    const double labelWidth = QFontMetrics(f).horizontalAdvance(text.toUpper()) + 40;
    const QRectF label(rect.center().x() - labelWidth / 2.0, rect.top() - (local ? 32 : 24),
                       labelWidth, local ? 24 : 18);
    p.fillPath(Theme::chamfer(label, 6), withAlpha(Theme::kField, 0.9));
    p.setFont(f);
    p.setPen(slotColor(spinner));
    p.drawText(label, Qt::AlignCenter, text.toUpper());

    // Fond rouge, zone verte centrale.
    QLinearGradient red(rect.topLeft(), rect.topRight());
    red.setColorAt(0.0, QColor(0x5a, 0x0f, 0x2a));
    red.setColorAt(0.5, Theme::kDanger);
    red.setColorAt(1.0, QColor(0x5a, 0x0f, 0x2a));
    const QPainterPath bar = Theme::skewed(rect, slant);
    p.fillPath(bar, red);
    p.setPen(QPen(QColor(0xff, 0x7a, 0x9b), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(bar);
    const double zone = side.done ? half : side.zone * half;
    const QRectF green(rect.center().x() - zone, rect.top(), 2.0 * zone, rect.height());
    QLinearGradient g(green.topLeft(), green.topRight());
    g.setColorAt(0.0, Theme::kAccent);
    g.setColorAt(1.0, Theme::kAccentBright);
    p.fillPath(Theme::skewed(green, std::min(slant, zone)), g);

    // Curseur.
    const double x = rect.center().x() + std::clamp(side.cursor, -1.0, 1.0) * half;
    const double grow = local ? 12.0 : 7.0;
    const double w = local ? 9.0 : 6.0;
    p.fillRect(QRectF(x - w, rect.top() - grow, 2 * w, rect.height() + 2 * grow), QColor(255, 255, 255, 50));
    p.setPen(QPen(QColor(0x05, 0x04, 0x0f), 1.5));
    p.setBrush(Qt::white);
    p.drawPolygon(QPolygonF{QPointF(x - w, rect.top() - grow), QPointF(x + w, rect.top() - grow),
                            QPointF(x + w * 0.4, rect.bottom() + grow), QPointF(x - w * 0.4, rect.bottom() + grow)});

    // Série finale : un losange par réussite à taille minimale.
    const double dot = local ? 8.0 : 5.0;
    for (int k = 0; k < state.config.duelStreak; ++k) {
        const QPointF c(rect.right() + 22 + k * (dot * 2.8), rect.center().y());
        p.setPen(QPen(Theme::kAccent, 1.5));
        p.setBrush(k < side.streak ? QBrush(Theme::kAccent) : QBrush(Qt::NoBrush));
        p.drawPolygon(QPolygonF{c + QPointF(0, -dot), c + QPointF(dot, 0), c + QPointF(0, dot),
                                c + QPointF(-dot, 0)});
    }
}

} // namespace toupie
