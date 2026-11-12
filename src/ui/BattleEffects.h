#pragma once

#include "game/Battle.h"

#include <QColor>
#include <QList>
#include <QPointF>

#include <functional>
#include <vector>

class QPainter;

namespace toupie {

/// Effets visuels d'un combat, partagés par l'arène et le combat de démonstration du
/// menu : étincelles des chocs (toupies, rebord, coups spéciaux), gerbes des duels,
/// traînées des toupies, effets des coups spéciaux en vol et états (gelée, sonnée).
class BattleEffects
{
public:
    /// Coordonnées du monde (rayons de stadium) → pixels.
    using ToScreen = std::function<QPointF(Vec2)>;

    void reset();
    /// Avance d'une image : nouveaux chocs, gerbes du duel en cours et, si les
    /// toupies bougent (combat lancé), allongement de leurs traînées.
    void update(double dt, const QList<ImpactEvent> &impacts, const std::vector<Spinner> &spinners,
                const Duel &duel, bool moving);

    /// Traînées lumineuses (sous les toupies).
    void paintTrails(QPainter &p, const std::vector<Spinner> &spinners, const ToScreen &toScreen,
                     double radius) const;
    /// Étincelles des chocs et des duels (par-dessus les toupies).
    void paintSparks(QPainter &p, const ToScreen &toScreen, double radius, double scale) const;

    /// Coups spéciaux en vol, chacun avec sa signature (anneaux, lance, givre…).
    static void paintProjectiles(QPainter &p, const std::vector<Projectile> &projectiles,
                                 const ToScreen &toScreen, double scale, double topRadius,
                                 double stadiumRadius, double time);
    /// Toupie gelée (coque de glace) ou sonnée (anneau et cadenas).
    static void paintStatus(QPainter &p, const Spinner &s, QPointF pos, double radius, double time);

private:
    struct Spark
    {
        QPointF position; // Coordonnées du monde.
        double age = 0.0;
        double intensity = 0.0;
        QColor color;     ///< Invalide : étincelle de choc classique.
    };
    /// Étincelle projetée par le frottement d'un duel.
    struct Ember
    {
        QPointF position; // Coordonnées du monde.
        QPointF velocity;
        double age = 0.0;
    };

    std::vector<QList<QPointF>> m_trails;
    QList<Spark> m_sparks;
    QList<Ember> m_embers;
};

} // namespace toupie
