#pragma once

namespace toupie {

/// Ensemble des constantes de gameplay.
///
/// Les distances sont exprimées en « rayons de stadium » (le stadium a un
/// rayon de 1), les durées en secondes et l'endurance (la rotation) en
/// points sur 100. Les formules qui utilisent ces valeurs sont documentées
/// dans Battle.cpp et dans le README.
struct BattleConfig
{
    // --- Stadium et lancement ---------------------------------------------
    double stadiumRadius = 1.0;
    double topRadius = 0.075;
    double startOffset = 0.55;     ///< Distance au centre des points de départ.
    double launchSpeed = 1.0;      ///< Vitesse initiale.

    // --- Mouvement -----------------------------------------------------------
    /// Le stadium est une cuvette : accélération vers le centre = k * distance.
    double slopeStrength = 1.3;
    /// Frottement visqueux : a = -friction * v.
    double friction = 0.7;
    /// Propulsion tangentielle due à la rotation (la toupie se déplace dans
    /// le sens inverse de sa rotation) : base + parAttaque * attaque.
    double orbitBase = 0.22;
    double orbitPerAttack = 0.022;
    /// En dessous de ce ratio de rotation, la toupie vacille.
    double wobbleThreshold = 0.25;
    double wobbleStrength = 1.2;

    // --- Endurance -----------------------------------------------------------
    double maxSpin = 100.0;
    /// Perte passive par seconde = base * ref / (ref + endurance) * (1 + drain * vitesse).
    double passiveBase = 3.2;
    double passiveRef = 8.0;
    double speedDrain = 0.5;

    // --- Chocs ----------------------------------------------------------------
    /// Dégâts = base * attaqueAdverse * impact * defRef / (defRef + défense).
    double damageBase = 0.75;
    double defenseRef = 8.0;
    /// impact = minImpact + vitesseDeRapprochement / impactRef.
    double minImpact = 0.4;
    double impactRef = 1.0;
    /// Recul = (base + parAttaque * attaqueAdverse + approche * vitesseRapprochement)
    ///         / (1 + parDéfense * défense).
    double knockbackBase = 0.25;
    double knockbackPerAttack = 0.05;
    double knockbackApproach = 0.5;
    double knockbackPerDefense = 0.12;
    /// Masse utilisée pour séparer les toupies qui se chevauchent.
    double massPerDefense = 0.1;
    /// Déviation latérale provoquée par la rotation au contact.
    double spinDeflection = 0.18;
    /// Deux toupies de même sens : surfaces de contact opposées, choc plus violent.
    double sameSpinKnockback = 1.25;
    /// Deux toupies de sens opposés : frottement, plus de perte d'endurance.
    double oppositeSpinDamage = 1.2;
    double contactCooldown = 0.15;

    // --- Dash -------------------------------------------------------------------
    double dashCost = 6.0;          ///< Points d'endurance consommés.
    double dashSpeed = 2.6;
    double dashDuration = 0.15;     ///< Distance du dash = vitesse * durée (~0,39).
    double dashCooldown = 1.8;
    double dashAttackMultiplier = 1.6;
    double dashBoostDuration = 0.35; ///< Durée du bonus d'attaque.
    double postDashSpeed = 0.45;

    // --- Partie -------------------------------------------------------------------
    double maxDuration = 90.0;      ///< Au-delà : victoire à l'endurance restante.
    double fixedStep = 1.0 / 240.0;

    double dashDistance() const { return dashSpeed * dashDuration; }
};

} // namespace toupie
