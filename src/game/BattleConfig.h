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
    // Valeurs réglées avec tools/balance_sim (voir README, section Équilibrage).

    // --- Stadium et lancement ---------------------------------------------
    double stadiumRadius = 1.0;
    double topRadius = 0.075;
    double startOffset = 0.55;     ///< Distance au centre des points de départ.
    double launchSpeed = 1.0;      ///< Vitesse initiale.
    /// Vitesse radiale minimale pour franchir le rebord du stadium ; en
    /// dessous, la toupie rebondit avec ce coefficient de restitution.
    double rimEscapeSpeed = 0.4981;
    double rimRestitution = 0.4;

    // --- Mouvement -----------------------------------------------------------
    /// Le stadium est une cuvette : accélération vers le centre = k * distance.
    double slopeStrength = 1.3;
    /// Frottement visqueux : a = -friction * v.
    double friction = 0.7;
    /// Propulsion tangentielle due à la rotation (la toupie se déplace dans
    /// le sens inverse de sa rotation) : base + parAttaque * attaque.
    double orbitBase = 0.22;
    double orbitPerAttack = 0.04;
    /// En dessous de ce ratio de rotation, la toupie vacille.
    double wobbleThreshold = 0.25;
    double wobbleStrength = 1.2;

    // --- Endurance -----------------------------------------------------------
    double maxSpin = 100.0;
    /// Perte passive par seconde =
    ///   base * max(plancher, 1 - parEndurance * endurance) * (1 + drain * vitesse).
    /// Les stats ont des effets linéaires : se spécialiser n'a pas de
    /// rendement décroissant par rapport à une répartition équilibrée.
    double passiveBase = 2.416;
    double passivePerEndurance = 0.045;
    double speedDrain = 0.2;

    // --- Chocs ----------------------------------------------------------------
    /// Dégâts = base * attaqueAdverse * impact * max(plancher, 1 - parDéfense * défense).
    double damageBase = 1.0;
    double damagePerDefense = 0.0557;
    /// impact = min(maxImpact, minImpact + vitesseDeRapprochement / impactRef).
    double minImpact = 0.741;
    double impactRef = 2.0;
    double maxImpact = 1.0;
    /// Recul = (base + parAttaque * attaqueAdverse + parMasse * défenseAdverse
    ///          + approche * vitesseRapprochement)
    ///         * max(plancher, 1 - parDéfense * défense).
    double knockbackBase = 0.12;
    double knockbackPerAttack = 0.06;
    /// Une toupie lourde (défense élevée) repousse plus fort.
    double knockbackPerAttackerDefense = 0.0231;
    double knockbackApproach = 0.2115;
    double knockbackPerDefense = 0.0298;
    /// Plancher des facteurs de réduction linéaires.
    double minReduction = 0.15;
    /// Masse utilisée pour séparer les toupies qui se chevauchent.
    double massPerDefense = 0.1;
    /// Déviation latérale provoquée par la rotation au contact.
    double spinDeflection = 0.1;
    /// Deux toupies de même sens : surfaces de contact opposées, choc plus violent.
    double sameSpinKnockback = 1.1;
    /// Deux toupies de sens opposés : frottement, plus de perte d'endurance.
    double oppositeSpinDamage = 1.2186;
    double contactCooldown = 0.25;
    /// Bonus du type équilibre : réduction (en fraction) de la perte
    /// d'endurance, des dégâts et du recul subis.
    double balanceStability = 0.0439;

    // --- Dash -------------------------------------------------------------------
    double dashCost = 5.868;        ///< Points d'endurance consommés.
    double dashSpeed = 2.6;
    double dashDuration = 0.15;     ///< Distance du dash = vitesse * durée (~0,39).
    double dashCooldown = 1.8;
    double dashAttackMultiplier = 1.2;
    double dashBoostDuration = 0.35; ///< Durée du bonus d'attaque.
    double postDashSpeed = 0.45;

    // --- Mêlée (3-4 toupies) ---------------------------------------------------
    /// Endurance rendue à la toupie qui provoque une élimination (dernier choc
    /// reçu par la victime il y a moins de knockoutWindow secondes).
    double knockoutReward = 35.0;
    double knockoutWindow = 4.0;

    // --- Partie -------------------------------------------------------------------
    double maxDuration = 90.0;      ///< Au-delà : victoire à l'endurance restante.
    double fixedStep = 1.0 / 240.0;

    double dashDistance() const { return dashSpeed * dashDuration; }
};

} // namespace toupie
