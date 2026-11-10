#pragma once

#include "core/Stats.h"

#include <QColor>
#include <QPointF>
#include <QString>

#include <array>

class QPainter;

namespace toupie {

struct Part;
class Top;

/// Fonctions de dessin partagées par l'arène et les aperçus.
namespace TopPainter {

/// Couleur associée à un type de toupie.
QColor typeColor(TopType type);
/// Couleur d'un coup spécial (blanc si inconnu).
QColor moveColor(const QString &special);

/// Dessine une toupie vue du dessus : roue métallique à silhouette propre,
/// partie intermédiaire à motif et boulon portant l'emblème de son coup spécial.
/// @param angle   angle de rotation propre (radians)
/// @param glow    intensité (0..1) d'un halo, utilisé pendant un dash
void paint(QPainter &painter, const Top &top, QPointF center, double radius, double angle,
           double glow = 0.0);

/// Pièce seule (cartes de l'atelier) : boulon, roue et intermédiaire vus du dessus,
/// pointe de profil, dans un cercle de rayon `radius`.
void paintPart(QPainter &painter, const Part &part, QPointF center, double radius);

/// Vue éclatée de profil (boulon, roue, intermédiaire, pointe de haut en bas), centrée
/// sur `origin.x()` à partir de `origin.y()`, de largeur `width` (hauteur ≈ width).
/// Renvoie un point d'accroche sur chaque pièce, indexé par PartKind.
std::array<QPointF, 4> paintProfile(QPainter &painter, const Top &top, QPointF origin, double width);

/// Emblème d'un coup spécial (onde, lance, givre…), centré, de demi-taille `size`.
void paintEmblem(QPainter &painter, const QString &special, QPointF center, double size,
                 const QColor &color, double penWidth = 2.0);

/// Petite flèche circulaire indiquant le sens de rotation.
void paintSpinArrow(QPainter &painter, SpinDirection direction, QPointF center, double radius,
                    const QColor &color);

} // namespace TopPainter
} // namespace toupie
