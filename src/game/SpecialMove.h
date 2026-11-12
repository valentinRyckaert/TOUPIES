#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace toupie {

/// Coup spécial, débloqué par le boulon d'une toupie.
///
/// Un coup spécial est une vague lancée depuis la toupie :
/// - « Vague » : un front qui file en ligne droite vers l'adversaire le plus
///   proche (sans le suivre : on peut l'esquiver en pilotant) et s'arrête au
///   premier adversaire touché ;
/// - « Anneau » : une onde circulaire qui s'élargit autour de la toupie et
///   touche tous les adversaires qu'elle rencontre jusqu'à sa portée.
/// Aucun coup ne cumule dégâts, recul, vitesse et effet : chacun a un point fort.
struct SpecialMove
{
    enum class Shape { Wave, Ring };
    enum class Effect {
        None,
        PullToCenter, ///< Aspire la cible vers le centre du stadium (sans le dépasser).
        Slow,         ///< Ralentit la cible et l'empêche de piloter.
        Teleport,     ///< Téléporte la cible près du bord.
        Drain,        ///< La toupie lanceuse récupère l'endurance infligée.
        Disarm,       ///< Plus de dash ni de coup spécial pendant un moment.
    };

    QString id;
    QString name;
    QString description;
    Shape shape = Shape::Wave;
    double damage = 0.0;         ///< Endurance retirée (avant défense).
    double knockback = 0.0;      ///< Vitesse de recul imprimée.
    double speed = 1.0;          ///< Vitesse de propagation (rayons de stadium / s).
    double range = 1.5;          ///< Distance maximale parcourue (rayon maximal d'un anneau).
    double width = 0.08;         ///< Demi-épaisseur du front de vague.
    Effect effect = Effect::None;
    double effectStrength = 0.0;
    double effectDuration = 0.0; ///< Secondes (effets temporaires).
    unsigned color = 0xffffff;   ///< Couleur d'affichage (0xRRGGBB).

    bool hitsEveryone() const { return shape == Shape::Ring; }

    static const QList<SpecialMove> &all();
    static std::optional<SpecialMove> find(const QString &id);

    /// Outils d'équilibrage uniquement : modifie une statistique d'un coup
    /// (champs : damage, knockback, speed, range, width, effectStrength,
    /// effectDuration). Retourne false si le coup ou le champ est inconnu.
    static bool overrideStat(const QString &id, const QString &field, double value);

private:
    static QList<SpecialMove> &registry();
};

} // namespace toupie
