#pragma once

#include "core/Top.h"
#include "game/Battle.h"
#include "game/BotController.h"

#include <QElapsedTimer>
#include <QList>
#include <QPointF>
#include <QTimer>
#include <QWidget>

#include <array>
#include <memory>

namespace toupie {

/// Vue du stadium : visée, compte à rebours, combat et résultat.
///
/// Le joueur contrôle la toupie 0 (à gauche), le bot la toupie 1.
/// Contrôles : souris ou ← → pour viser, clic / Entrée pour lancer,
/// clic / Espace pour dasher pendant le combat.
class ArenaWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Phase { Idle, Aiming, Countdown, Fighting, Finished };
    Q_ENUM(Phase)

    explicit ArenaWidget(QWidget *parent = nullptr);
    ~ArenaWidget() override;

    void startMatch(const Top &player, const Top &opponent, Difficulty difficulty);
    void restart();
    void stop();

    Phase phase() const { return m_phase; }

    QSize sizeHint() const override { return {720, 720}; }

signals:
    void matchFinished(int winner);
    void phaseChanged(toupie::ArenaWidget::Phase phase);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Spark
    {
        QPointF position; // Coordonnées du monde.
        double age = 0.0;
        double intensity = 0.0;
    };

    void tick();
    void setPhase(Phase phase);
    void confirmLaunch();
    void tryDash();

    double scale() const;
    QPointF toScreen(Vec2 world) const;
    Vec2 toWorld(QPointF screen) const;

    void paintStadium(QPainter &p);
    void paintAim(QPainter &p);
    void paintSpinners(QPainter &p);
    void paintHud(QPainter &p);
    void paintSpinnerHud(QPainter &p, const Spinner &s, const QRectF &rect, bool player,
                         bool alignRight);
    void paintOverlay(QPainter &p);

    Top m_player;
    Top m_opponent;
    Difficulty m_difficulty = Difficulty::Normal;

    std::unique_ptr<Battle> m_battle;
    std::unique_ptr<BotController> m_bot;
    Phase m_phase = Phase::Idle;
    double m_aimAngle = 0.0;
    double m_botAngle = 0.0;
    double m_countdown = 0.0;
    double m_flash = 0.0;

    std::array<QList<QPointF>, 2> m_trails;
    QList<Spark> m_sparks;

    QTimer m_timer;
    QElapsedTimer m_clock;
};

} // namespace toupie
