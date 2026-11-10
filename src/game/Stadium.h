#pragma once

#include "game/BattleConfig.h"
#include "game/Vec2.h"

#include <QList>
#include <QString>

#include <optional>

namespace toupie {

/// Forme géométrique d'un stadium, centrée sur l'origine.
///
/// level(p) vaut 0 au centre et 1 sur le bord : un point est hors du stadium
/// quand level(p) > 1. Le paramètre `inset` réduit la forme (rayon d'une
/// toupie) pour savoir si une toupie touche le bord.
struct StadiumGeometry
{
    enum class Kind { Circle, Polygon, Ellipse };

    Kind kind = Kind::Circle;
    double radius = 1.0; ///< Rayon (cercle), rayon circonscrit (polygone), demi-grand axe (ellipse).
    int sides = 8;       ///< Polygone uniquement.
    double aspect = 1.0; ///< Ellipse uniquement : demi-petit axe / demi-grand axe (axe vertical).

    double level(Vec2 p, double inset = 0.0) const;
    /// Normale sortante du bord le plus proche de p.
    Vec2 outwardNormal(Vec2 p) const;
    /// Ramène p à l'intérieur de la forme réduite de `inset`.
    Vec2 projectInside(Vec2 p, double inset) const;
    /// Distance minimale entre le centre et le bord.
    double innerRadius() const;
    /// Contour (pour l'affichage).
    QList<Vec2> outline(int segments = 96) const;

private:
    double apothem() const;
    Vec2 sideNormal(int index) const;
};

/// Type de stadium : forme, présence d'un rebord et modificateurs de gameplay.
///
/// Les modificateurs sont des multiplicateurs (1 = neutre) appliqués aux
/// constantes de BattleConfig.
struct Stadium
{
    QString id;
    QString name;
    QString description;
    QString codename;           ///< Nom de code affiché sous le nom (« Noyau Holo »…).
    unsigned accent = 0x34d6ff; ///< Couleur signature (0xRRGGBB) de l'interface.

    StadiumGeometry::Kind shape = StadiumGeometry::Kind::Circle;
    int sides = 8;
    double aspect = 1.0;
    double size = 1.0;          ///< Échelle du stadium.
    bool hasRim = true;         ///< Sans rebord, une toupie qui atteint le bord tombe.
    double rimHeight = 1.0;     ///< Vitesse nécessaire pour franchir le rebord.

    double slope = 1.0;         ///< Pente de la cuvette.
    double friction = 1.0;      ///< Frottement (adhérence de la surface).
    double speed = 1.0;         ///< Propulsion due à la rotation.
    double enduranceDrain = 1.0;///< Usure passive de la rotation.
    double damage = 1.0;        ///< Dégâts des chocs.
    double knockback = 1.0;     ///< Recul des chocs.
    double dashSpeed = 1.0;     ///< Vitesse (donc distance) des dashs.
    double dashPower = 1.0;     ///< Bonus d'attaque des dashs.

    /// Couleurs d'affichage (0xRRGGBB).
    unsigned floorCenter = 0xd7dce6;
    unsigned floorEdge = 0x69708a;
    unsigned rimColor = 0x464a5c;

    /// Constantes de combat modifiées par ce stadium.
    BattleConfig apply(const BattleConfig &base) const;
    /// Géométrie pour une configuration déjà modifiée par apply().
    StadiumGeometry geometry(const BattleConfig &applied) const;

    /// Stadiums disponibles ; le premier est le stadium de référence.
    static const QList<Stadium> &all();
    static const Stadium &classic();
    static std::optional<Stadium> find(const QString &id);

    /// Outils d'équilibrage uniquement : modifie un modificateur d'un stadium
    /// (size, rimHeight, slope, friction, speed, enduranceDrain, damage,
    /// knockback, dashSpeed, dashPower). Retourne false si inconnu.
    static bool overrideStat(const QString &id, const QString &field, double value);

private:
    static QList<Stadium> &registry();
};

} // namespace toupie
