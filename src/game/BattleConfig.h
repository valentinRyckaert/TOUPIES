#pragma once

namespace toupie {

/// Duel entre deux toupies qui se percutent en plein dash.
enum class DuelMode {
    Timing, ///< Appuyer quand le curseur traverse la zone verte (zone de plus en plus étroite).
    Rush,   ///< Rafale : tir à la corde, chaque appui pousse le curseur vers l'adversaire.
    None,   ///< Pas de duel : choc ordinaire.
};

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
    double rimEscapeSpeed = 0.4223;
    double rimRestitution = 0.4;
    /// Rebond contre le rebord : perte d'endurance = rimDamage × vitesse vers le
    /// rebord, réduite par la défense comme un choc. En dessous de
    /// rimDamageThreshold (toupie qui longe le bord), rien n'est perdu.
    double rimDamage = 4.0;
    double rimDamageThreshold = 0.08;

    // --- Mouvement -----------------------------------------------------------
    /// Le stadium est une cuvette : accélération vers le centre = k * distance.
    double slopeStrength = 1.3;
    /// Frottement visqueux : a = -friction * v.
    double friction = 0.7;
    /// Propulsion tangentielle due à la rotation (la toupie se déplace dans
    /// le sens inverse de sa rotation) : base + parAttaque * attaque.
    double orbitBase = 0.1924;
    double orbitPerAttack = 0.0049;
    /// Attraction constante vers le centre : sans action contraire (pilotage,
    /// chocs), les toupies finissent par se rapprocher du centre.
    double centerPull = 0.1837;
    /// Pilotage aux flèches : légère accélération dans la direction voulue.
    double steerAcceleration = 0.3;
    /// Force de lancer : multiplicateur de launchSpeed choisi par le joueur.
    double launchPowerMin = 0.5;
    double launchPowerMax = 1.5;
    /// En dessous de ce ratio de rotation, la toupie vacille.
    double wobbleThreshold = 0.25;
    double wobbleStrength = 1.2;

    // --- Endurance -----------------------------------------------------------
    double maxSpin = 100.0;
    /// Perte passive par seconde =
    ///   base * max(plancher, 1 - parEndurance * endurance) * (1 + drain * vitesse).
    /// Les stats ont des effets linéaires : se spécialiser n'a pas de
    /// rendement décroissant par rapport à une répartition équilibrée.
    double passiveBase = 1.8491;
    double passivePerEndurance = 0.03;
    double speedDrain = 0.7327;

    // --- Chocs ----------------------------------------------------------------
    /// Dégâts = base * attaqueAdverse * impact * max(plancher, 1 - parDéfense * défense).
    double damageBase = 0.4;
    double damagePerDefense = 0.0482;
    /// impact = min(maxImpact, minImpact + vitesseDeRapprochement / impactRef).
    double minImpact = 0.8969;
    double impactRef = 2.0;
    double maxImpact = 1.1305;
    /// Recul = (base + parAttaque * attaqueAdverse + parMasse * défenseAdverse
    ///          + approche * vitesseRapprochement)
    ///         * max(plancher, 1 - parDéfense * défense).
    double knockbackBase = 0.12;
    double knockbackPerAttack = 0.0715;
    /// Une toupie lourde (défense élevée) repousse plus fort.
    double knockbackPerAttackerDefense = 0.0418;
    double knockbackApproach = 0.1427;
    double knockbackPerDefense = 0.0279;
    /// Plancher des facteurs de réduction linéaires.
    double minReduction = 0.15;
    /// Masse utilisée pour séparer les toupies qui se chevauchent.
    double massPerDefense = 0.1;
    /// Déviation latérale provoquée par la rotation au contact.
    double spinDeflection = 0.1;
    /// Deux toupies de même sens : surfaces de contact opposées, choc plus violent.
    double sameSpinKnockback = 1.1;
    /// Deux toupies de sens opposés : frottement, plus de perte d'endurance.
    double oppositeSpinDamage = 1.2014;
    double contactCooldown = 0.2337;
    /// Bonus du type équilibre : réduction (en fraction) de la perte
    /// d'endurance, des dégâts et du recul subis.
    double balanceStability = 0.02;

    // --- Dash -------------------------------------------------------------------
    double dashCost = 3.5722;        ///< Points d'endurance consommés.
    double dashSpeed = 2.6;
    double dashDuration = 0.15;     ///< Distance du dash = vitesse * durée (~0,39).
    double dashCooldown = 1.8;
    double dashAttackMultiplier = 1.1618;
    double dashBoostDuration = 0.35; ///< Durée du bonus d'attaque.
    double postDashSpeed = 0.45;

    // --- Coups spéciaux ------------------------------------------------------
    /// Chaque toupie ne peut lancer le coup spécial de son boulon qu'une seule
    /// fois par partie ; le lancer lui coûte cette fraction de son endurance
    /// courante (cher en début de partie, mais assez léger pour servir en
    /// milieu de partie, quand son effet compte, plutôt que pour achever).
    double specialCost = 0.25;
    double specialDamageScale = 1.0; ///< Multiplicateur des dégâts des coups spéciaux.
    double specialKnockbackScale = 1.0;
    /// Toupie aspirée (vortex) : rapidité (1/s) avec laquelle sa vitesse rejoint
    /// celle de l'aspiration.
    double pullGrip = 12.0;
    /// Toupie gelée (souffle de givre) : sa perte passive est multipliée par
    /// 1 + slowWear × intensité du gel (le froid ronge la rotation).
    double slowWear = 2.0;
    bool specialsEnabled = true;

    // --- Duel (deux toupies qui se percutent en plein dash) --------------------
    DuelMode duelMode = DuelMode::Timing;
    /// Rafale : durée du duel, et avance (en appuis) qui le gagne aussitôt.
    double rushDuration = 3.5;
    int rushLimit = 12;
    /// Timing :
    /// Chaque duelliste a une barre de -1 à 1 parcourue par un curseur ; il faut
    /// appuyer quand le curseur est dans la zone verte centrée sur 0, dont la
    /// demi-largeur rétrécit à chaque réussite jusqu'à duelZoneMin.
    double duelZoneStart = 0.45;
    double duelZoneMin = 0.14;
    double duelZoneShrink = 0.72;
    double duelCursorSpeed = 2.2;   ///< Unités de barre par seconde.
    double duelSpeedGain = 1.06;    ///< Accélération du curseur à chaque réussite.
    int duelStreak = 3;             ///< Réussites à taille minimale pour un nul.
    double duelTimeout = 3.0;       ///< Sans appui pendant ce délai : duel perdu.
    /// Le perdant subit ce multiple des dégâts du dash adverse.
    double duelDamageMultiplier = 1.5;

    // --- Mêlée (3-4 toupies) ---------------------------------------------------
    /// Endurance rendue à la toupie qui provoque une élimination (dernier choc
    /// reçu par la victime il y a moins de knockoutWindow secondes).
    double knockoutReward = 35.0;
    double knockoutWindow = 4.0;

    // --- Partie -------------------------------------------------------------------
    /// Durée maximale (0 = aucune) ; au-delà, victoire à l'endurance restante.
    double maxDuration = 0.0;
    double fixedStep = 1.0 / 240.0;

    double dashDistance() const { return dashSpeed * dashDuration; }
};

} // namespace toupie
