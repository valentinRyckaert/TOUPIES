#pragma once

#include "session/MatchSession.h"

#include <QElapsedTimer>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <vector>

namespace toupie {

/// Vue du stadium : visée, compte à rebours, combat et résultat.
///
/// L'arène affiche l'état d'une MatchSession (solo, hôte ou client réseau)
/// et lui transmet les actions du joueur local.
/// Contrôles : souris ou ← → pour viser, clic / Entrée pour lancer,
/// clic / Espace pour dasher pendant le combat, R pour demander une revanche.
class ArenaWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ArenaWidget(QWidget *parent = nullptr);
    ~ArenaWidget() override;

    /// Session affichée (non possédée) ; nullptr pour arrêter.
    void setSession(MatchSession *session);
    MatchSession *session() const { return m_session; }

    /// Couleur associée à une place (0 à 3).
    static QColor slotColor(int slot);

    QSize sizeHint() const override { return {760, 760}; }

signals:
    void matchFinished(int winner);
    void rematchRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
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
    void resetVisuals();
    void confirmLaunch();
    void tryDash();

    double scale() const;
    QPointF center() const;
    QPointF toScreen(Vec2 world) const;
    Vec2 toWorld(QPointF screen) const;

    void paintStadium(QPainter &p);
    void paintAim(QPainter &p);
    void paintSpinners(QPainter &p);
    void paintHud(QPainter &p);
    void paintSpinnerHud(QPainter &p, int index, const QRectF &rect);
    void paintOverlay(QPainter &p);

    QPointer<MatchSession> m_session;
    MatchPhase m_lastPhase = MatchPhase::Aiming;
    double m_aimAngle = 0.0;
    double m_flash = 0.0;

    std::vector<QList<QPointF>> m_trails;
    QList<Spark> m_sparks;
    QPixmap m_stadiumCache;
    QString m_stadiumCacheKey;

    QTimer m_timer;
    QElapsedTimer m_clock;
};

} // namespace toupie
