#pragma once

#include "session/MatchSession.h"

#include <QElapsedTimer>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QSet>
#include <QTimer>
#include <QWidget>

#include <vector>

namespace toupie {

/// Vue du stadium : visée, compte à rebours, combat et résultat.
///
/// L'arène affiche l'état d'une MatchSession (solo, hôte ou client réseau)
/// et lui transmet les actions du joueur local.
/// Les touches et boutons de souris viennent de Controls (page Paramètres) ;
/// la souris vise toujours pendant la phase de visée.
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
    /// Pixels par unité de monde pour un stadium classique de rayon 1 dans une arène de
    /// cette taille (même échelle pour tous les stadiums, qui ont la même surface).
    static double unitScale(const QSizeF &arena);
    /// Légende en haut à droite, sous le stadium (bilan des victoires, mode de jeu…).
    void setCaption(const QString &caption);

signals:
    void matchFinished(int winner);
    void rematchRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    struct Spark
    {
        QPointF position; // Coordonnées du monde.
        double age = 0.0;
        double intensity = 0.0;
        QColor color;          ///< Invalide : étincelle de choc classique.
    };
    /// Étincelle projetée par le frottement d'un duel.
    struct Ember
    {
        QPointF position; // Coordonnées du monde.
        QPointF velocity;
        double age = 0.0;
    };

    void tick();
    void resetVisuals();
    void confirmLaunch();
    void tryDash();
public:
    /// Lance le coup spécial du joueur local (touche E ou bouton).
    void trySpecial();
private:
    void updateSteering();
    /// Applique les actions associées à une entrée (touche ou bouton de
    /// souris, voir Controls) ; false si l'entrée n'est associée à rien.
    bool pressInput(int code, bool autoRepeat);
    /// Force de lancer correspondant à une longueur de flèche (unités du monde).
    double powerForLength(double length) const;
    double lengthForPower(double power) const;
    void paintProjectiles(QPainter &p);

    double scale() const;
    QPointF center() const;
    QPointF toScreen(Vec2 world) const;
    Vec2 toWorld(QPointF screen) const;

    void paintStadium(QPainter &p);
    void paintAim(QPainter &p);
    void paintSpinners(QPainter &p);
    void paintHud(QPainter &p);
    void paintSpinnerHud(QPainter &p, int index, const QRectF &rect, bool mirrored);
    /// Carte du coup spécial du joueur local (cliquable).
    void paintSpecial(QPainter &p, const QRectF &rect);
    void paintOverlay(QPainter &p);
    void paintDuel(QPainter &p);
    void paintTutorial(QPainter &p);
    void paintDuelBar(QPainter &p, const QRectF &rect, int spinner, const DuelSide &side,
                      bool local);
    /// Duel en rafale : barre de tir à la corde.
    void paintRush(QPainter &p, const QRectF &rect);

    QPointer<MatchSession> m_session;
    MatchPhase m_lastPhase = MatchPhase::Aiming;
    double m_aimAngle = 0.0;
    double m_aimPower = 1.0;
    QSet<int> m_steerKeys;
    double m_flash = 0.0;

    std::vector<QList<QPointF>> m_trails;
    QList<Spark> m_sparks;
    QList<Ember> m_embers;
    QString m_duelResult;      ///< Bandeau de fin de duel.
    QColor m_duelResultColor;
    double m_duelResultTimer = 0.0;
    QString m_caption;
    QRectF m_specialRect; ///< Carte du coup spécial (vide si absente).
    QPixmap m_stadiumCache;
    QString m_stadiumCacheKey;

    QTimer m_timer;
    QElapsedTimer m_clock;
};

} // namespace toupie
