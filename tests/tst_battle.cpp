#include "core/PartCatalog.h"
#include "game/Battle.h"
#include "game/BotController.h"
#include "game/MatchRules.h"

#include <QSet>

#include <random>
#include <QTest>

#include <cmath>

using namespace toupie;

namespace {

/// advance() plafonne chaque appel à 0,1 s : on simule par images.
void run(Battle &battle, double seconds)
{
    for (double t = 0.0; t < seconds && !battle.isFinished(); t += 1.0 / 60.0)
        battle.advance(1.0 / 60.0);
}

Top makeTop(const char *wheel, const char *middle, const char *tip)
{
    const PartCatalog &c = PartCatalog::instance();
    return Top(QStringLiteral("test"), *c.find(wheel), *c.find(middle), *c.find(tip));
}

Top attackTop() { return makeTop("RM2", "PI2", "P2"); }    // 14/4/5, horaire
Top defenseTop() { return makeTop("RM3", "PI3", "P1"); }   // 4/14/5, anti-horaire
Top enduranceTop() { return makeTop("RM4", "PI4", "P3"); } // 4/4/15, anti-horaire

} // namespace

class TestBattle : public QObject
{
    Q_OBJECT

private slots:
    void enduranceReducesPassiveLoss();
    void defenseReducesDamageAndKnockback();
    void dashBoostsAttackAndCostsEndurance();
    void cannotDashDuringCooldown();
    void topsMoveOppositeToTheirSpin();
    void slowTopBouncesOnRim();
    void fastTopIsRingedOut();
    void topWithoutEnduranceLoses();
    void simulationIsDeterministic();
    void botMatchTerminates();
    void harderBotsWinMoreOften();
    void botKeepsSpecialForLater();
    void stadiumGeometry();
    void stadiumsAreDistinct();
    void topFallsOffStadiumWithoutRim();
    void fourPlayerMatchProducesRanking();
    void forfeitEliminatesPlayer();
    void dashTargetsNearestOrChosenOpponent();
    void rulesModifyConstants();
    void launchPowerScalesSpeed();
    void steeringChangesTrajectory();
    void topsDriftTowardCenter();
    void specialIsSingleUseAndCostsPartOfEndurance();
    void waveHitsTargetAndAppliesEffect_data();
    void waveHitsTargetAndAppliesEffect();
    void ringHitsEveryOpponentInRange();
    void disarmBlocksDash();
    void simultaneousDashesStartDuel();
    void duelMissCostsOneAndHalfDashDamage();
    void duelDrawAfterFinalStreak();
    void idleDuellistLoses();
    void harderBotsWinDuels();
    void finalEliminationKeepsWinnerEndurance();
};

void TestBattle::enduranceReducesPassiveLoss()
{
    Battle battle(enduranceTop(), attackTop());
    const double enduranceLoss = battle.passiveLossPerSecond(battle.spinner(0));
    const double attackLoss = battle.passiveLossPerSecond(battle.spinner(1));
    QVERIFY(enduranceLoss > 0.0);
    QVERIFY(enduranceLoss < attackLoss);
}

void TestBattle::defenseReducesDamageAndKnockback()
{
    Battle battle(attackTop(), defenseTop());
    const Spinner &attacker = battle.spinner(0);
    const Spinner &defender = battle.spinner(1);

    // Ce que le défenseur subit vs ce que subirait une toupie peu défensive.
    Battle reference(attackTop(), enduranceTop());
    const Spinner &fragile = reference.spinner(1);

    QVERIFY(battle.collisionDamage(attacker, defender, 1.0)
            < reference.collisionDamage(attacker, fragile, 1.0));
    QVERIFY(battle.knockback(attacker, defender, 1.0)
            < reference.knockback(attacker, fragile, 1.0));
    // Plus le choc est rapide, plus il fait mal.
    QVERIFY(battle.collisionDamage(attacker, defender, 1.0)
            > battle.collisionDamage(attacker, defender, 0.0));
}

void TestBattle::dashBoostsAttackAndCostsEndurance()
{
    Battle battle(attackTop(), defenseTop());
    battle.launch(0.0, M_PI);
    const double before = battle.spinner(0).spin;
    const double attackBefore = battle.effectiveAttack(battle.spinner(0));

    QVERIFY(battle.requestDash(0));
    QCOMPARE(battle.spinner(0).spin, before - battle.config().dashCost);
    QVERIFY(battle.spinner(0).isDashing());
    QVERIFY(battle.effectiveAttack(battle.spinner(0)) > attackBefore);

    // Le dash se dirige vers l'adversaire.
    const Vec2 toOpponent = battle.spinner(1).position - battle.spinner(0).position;
    QVERIFY(battle.spinner(0).velocity.dot(toOpponent) > 0.0);
}

void TestBattle::cannotDashDuringCooldown()
{
    Battle battle(attackTop(), defenseTop());
    QVERIFY(!battle.requestDash(0)); // Pas encore lancé.
    battle.launch(0.0, M_PI);
    QVERIFY(battle.requestDash(0));
    QVERIFY(!battle.requestDash(0));
    battle.advance(0.05);
    QVERIFY(!battle.canDash(0));
}

void TestBattle::topsMoveOppositeToTheirSpin()
{
    BattleConfig config;
    config.launchSpeed = 0.0;
    // Toupie 0 horaire (à gauche du centre), toupie 1 anti-horaire (à droite).
    Battle battle(attackTop(), defenseTop(), config, 1);
    battle.launch(0.0, 0.0);
    battle.advance(0.2);

    // À gauche du centre, aller vers le bas de l'écran = sens anti-horaire.
    QVERIFY(battle.spinner(0).velocity.y > 0.0);
    // À droite du centre, aller vers le bas de l'écran = sens horaire.
    QVERIFY(battle.spinner(1).velocity.y > 0.0);
    QCOMPARE(battle.spinner(0).spinSign, 1);
    QCOMPARE(battle.spinner(1).spinSign, -1);
}

void TestBattle::slowTopBouncesOnRim()
{
    BattleConfig config;
    config.launchSpeed = config.rimEscapeSpeed * 0.5;
    config.slopeStrength = 0.0;
    config.friction = 0.0;
    config.orbitBase = config.orbitPerAttack = 0.0;
    config.startOffset = config.stadiumRadius - config.topRadius - 0.01;
    Battle battle(defenseTop(), defenseTop(), config, 1);
    battle.launch(M_PI, 0.0); // Les deux toupies partent vers l'extérieur.
    run(battle, 1.5);
    QVERIFY(!battle.isFinished());
    QVERIFY(battle.spinner(0).position.length() <= config.stadiumRadius);
}

void TestBattle::fastTopIsRingedOut()
{
    BattleConfig config;
    config.launchSpeed = 4.0;
    Battle battle(attackTop(), defenseTop(), config, 1);
    // La toupie 0 part vers l'extérieur, la 1 vers le centre.
    battle.launch(M_PI, M_PI);
    for (int i = 0; i < 600 && !battle.isFinished(); ++i)
        battle.advance(1.0 / 60.0);
    QVERIFY(battle.isFinished());
    QCOMPARE(battle.endReason(), Battle::EndReason::RingOut);
    QCOMPARE(battle.winner(), 1);
    QCOMPARE(battle.spinner(0).status, Spinner::Status::RingOut);
}

void TestBattle::topWithoutEnduranceLoses()
{
    BattleConfig config;
    config.passiveBase = 60.0;
    config.launchSpeed = 0.0;
    config.orbitBase = config.orbitPerAttack = 0.0;
    Battle battle(attackTop(), enduranceTop(), config, 1);
    battle.launch(0.0, 0.0);
    for (int i = 0; i < 600 && !battle.isFinished(); ++i)
        battle.advance(1.0 / 60.0);
    QVERIFY(battle.isFinished());
    QCOMPARE(battle.endReason(), Battle::EndReason::SpinOut);
    QCOMPARE(battle.winner(), 1); // L'endurance tient plus longtemps.
}

void TestBattle::simulationIsDeterministic()
{
    auto run = [] {
        Battle battle(attackTop(), enduranceTop(), BattleConfig{}, 1234);
        BotController a(0, Difficulty::Hard, 1);
        BotController b(1, Difficulty::Hard, 2);
        battle.launch(a.chooseLaunchAngle(battle), b.chooseLaunchAngle(battle));
        while (!battle.isFinished()) {
            a.update(battle, 1.0 / 60.0);
            b.update(battle, 1.0 / 60.0);
            battle.advance(1.0 / 60.0);
        }
        return std::make_tuple(battle.winner(), battle.elapsed(), battle.spinner(0).spin);
    };
    QCOMPARE(run(), run());
}

void TestBattle::botMatchTerminates()
{
    for (Difficulty d : {Difficulty::Easy, Difficulty::Normal, Difficulty::Hard}) {
        Battle battle(defenseTop(), enduranceTop(), BattleConfig{}, 99);
        BotController a(0, d, 3);
        BotController b(1, d, 4);
        const double angle = a.chooseLaunchAngle(battle);
        QVERIFY(std::isfinite(angle));
        battle.launch(angle, b.chooseLaunchAngle(battle));
        int frames = 0;
        while (!battle.isFinished() && frames < 60 * 300) {
            a.update(battle, 1.0 / 60.0);
            b.update(battle, 1.0 / 60.0);
            battle.advance(1.0 / 60.0);
            ++frames;
        }
        QVERIFY(battle.isFinished());
    }
}

void TestBattle::stadiumGeometry()
{
    StadiumGeometry circle;
    QCOMPARE(circle.level({0.5, 0.0}), 0.5);
    QVERIFY(circle.level({0.8, 0.0}, 0.25) > 1.0);
    const Vec2 n = circle.outwardNormal({0.0, -2.0});
    QVERIFY(std::abs(n.y + 1.0) < 1e-9);

    StadiumGeometry hexagon;
    hexagon.kind = StadiumGeometry::Kind::Polygon;
    hexagon.sides = 6;
    const double apothem = std::cos(M_PI / 6);
    QVERIFY(std::abs(hexagon.innerRadius() - apothem) < 1e-9);
    QVERIFY(hexagon.level({apothem * 0.99, 0.0}) < 1.0);
    QVERIFY(hexagon.level({apothem * 1.01, 0.0}) > 1.0);
    // Un sommet (rayon 1) est sur le bord.
    QVERIFY(std::abs(hexagon.level(Vec2::fromAngle(M_PI / 6)) - 1.0) < 1e-9);
    const Vec2 inside = hexagon.projectInside({2.0, 0.3}, 0.1);
    QVERIFY(hexagon.level(inside, 0.1) <= 1.0 + 1e-9);

    StadiumGeometry ellipse;
    ellipse.kind = StadiumGeometry::Kind::Ellipse;
    ellipse.aspect = 0.5;
    QVERIFY(ellipse.level({0.9, 0.0}) < 1.0);
    QVERIFY(ellipse.level({0.0, 0.6}) > 1.0);
    QCOMPARE(ellipse.innerRadius(), 0.5);
}

void TestBattle::stadiumsAreDistinct()
{
    QSet<QString> ids;
    for (const Stadium &stadium : Stadium::all()) {
        QVERIFY(!ids.contains(stadium.id));
        ids.insert(stadium.id);
        QVERIFY(!stadium.name.isEmpty() && !stadium.description.isEmpty());
    }
    QVERIFY(ids.size() >= 5);
    QVERIFY(Stadium::find(QStringLiteral("terre")).has_value());
    QVERIFY(!Stadium::find(QStringLiteral("inconnu")).has_value());

    // Le stadium classique ne modifie rien.
    const BattleConfig base;
    const BattleConfig applied = Stadium::classic().apply(base);
    QCOMPARE(applied.friction, base.friction);
    QCOMPARE(applied.damageBase, base.damageBase);
    QCOMPARE(applied.dashAttackMultiplier, base.dashAttackMultiplier);

    // Les modificateurs sont bien appliqués.
    const Stadium sand = *Stadium::find(QStringLiteral("sable"));
    const BattleConfig sandy = sand.apply(base);
    QVERIFY(sandy.friction > base.friction);
    QVERIFY(sandy.passiveBase > base.passiveBase);
    QVERIFY(sandy.dashAttackMultiplier > base.dashAttackMultiplier);
}

void TestBattle::topFallsOffStadiumWithoutRim()
{
    BattleConfig config;
    config.launchSpeed = config.rimEscapeSpeed * 0.6; // Trop lent pour franchir le rebord.
    config.slopeStrength = 0.0;
    config.friction = 0.0;
    config.orbitBase = config.orbitPerAttack = 0.0;
    config.startOffset = 0.85;

    Battle withRim({defenseTop(), defenseTop()}, Stadium::classic(), config, 1);
    withRim.launch(M_PI, 0.0);
    run(withRim, 1.5);
    QVERIFY(!withRim.isFinished());

    Stadium open = Stadium::classic();
    open.hasRim = false;
    Battle withoutRim({defenseTop(), defenseTop()}, open, config, 1);
    withoutRim.launch(M_PI, 0.0);
    run(withoutRim, 1.5);
    QVERIFY(withoutRim.isFinished());
    QVERIFY(withoutRim.endReason() == Battle::EndReason::RingOut);
}

void TestBattle::fourPlayerMatchProducesRanking()
{
    for (const Stadium &stadium : Stadium::all()) {
        Battle battle({attackTop(), defenseTop(), enduranceTop(), attackTop()}, stadium,
                      BattleConfig{}, 11);
        QCOMPARE(battle.playerCount(), 4);
        // Départs répartis autour du centre, tous dans le stadium.
        for (int i = 0; i < 4; ++i)
            QVERIFY(battle.geometry().level(battle.startPosition(i)) < 0.9);

        std::vector<BotController> bots;
        QList<double> angles;
        for (int i = 0; i < 4; ++i) {
            bots.emplace_back(i, Difficulty::Normal, 100 + i);
            angles.append(bots.back().chooseLaunchAngle(battle));
        }
        battle.launch(angles);
        while (!battle.isFinished()) {
            for (BotController &bot : bots)
                bot.update(battle, 1.0 / 60.0);
            battle.advance(1.0 / 60.0);
        }
        QCOMPARE(battle.ranking().size(), 4);
        QCOMPARE(QSet<int>(battle.ranking().begin(), battle.ranking().end()).size(), 4);
        if (battle.winner() >= 0)
            QCOMPARE(battle.ranking().first(), battle.winner());
    }
}

void TestBattle::forfeitEliminatesPlayer()
{
    Battle battle({attackTop(), defenseTop(), enduranceTop()}, Stadium::classic(), BattleConfig{}, 3);
    battle.forfeit(1); // Avant le lancement.
    battle.launch({0.0, 0.0, 0.0});
    QVERIFY(battle.spinner(1).status == Spinner::Status::Forfeit);
    QVERIFY(!battle.isFinished());
    battle.forfeit(2);
    QVERIFY(battle.isFinished());
    QCOMPARE(battle.winner(), 0);
    QVERIFY(battle.endReason() == Battle::EndReason::Forfeit);
    QCOMPARE(battle.ranking(), (QList<int>{0, 2, 1}));
}

void TestBattle::dashTargetsNearestOrChosenOpponent()
{
    BattleConfig config;
    config.launchSpeed = 0.0;
    // 4 joueurs : 0 à gauche, 1 en haut, 2 à droite, 3 en bas.
    Battle battle({attackTop(), defenseTop(), enduranceTop(), defenseTop()}, Stadium::classic(),
                  config, 1);
    battle.launch({0.0, 0.0, 0.0, 0.0});
    const int nearest = battle.nearestOpponent(0);
    QVERIFY(nearest == 1 || nearest == 3);

    QVERIFY(battle.requestDash(0, 2));
    const Vec2 toRight = (battle.spinner(2).position - battle.spinner(0).position).normalized();
    QVERIFY(battle.spinner(0).dashDirection.dot(toRight) > 0.99);
}

namespace {
Top withBolt(Top top, const char *boltId)
{
    top.setPart(*PartCatalog::instance().find(QString::fromLatin1(boltId)));
    return top;
}

/// Combat figé (aucun mouvement propre) pour tester les coups spéciaux.
BattleConfig staticConfig()
{
    BattleConfig c;
    c.launchSpeed = 0.0;
    c.slopeStrength = 0.0;
    c.centerPull = 0.0;
    c.orbitBase = c.orbitPerAttack = 0.0;
    return c;
}

/// Fait avancer jusqu'à ce que `done` soit vrai (au plus `seconds`).
template<typename Predicate>
void runUntil(Battle &battle, double seconds, Predicate done)
{
    for (double t = 0.0; t < seconds && !done() && !battle.isFinished(); t += 1.0 / 240.0)
        battle.advance(1.0 / 240.0);
}
} // namespace

void TestBattle::rulesModifyConstants()
{
    const BattleConfig base;
    MatchRules rules;
    QVERIFY(rules.isDefault());
    QCOMPARE(base.maxDuration, 0.0); // Aucune limite de temps par défaut.

    rules.speed = 2.0;
    rules.wear = 0.5;
    rules.maxEndurance = 200;
    rules.timeLimit = 60;
    rules.steering = 0.0;
    rules.specials = false;
    const BattleConfig c = rules.apply(base);
    QCOMPARE(c.launchSpeed, base.launchSpeed * 2.0);
    QCOMPARE(c.passiveBase, base.passiveBase * 0.5);
    QCOMPARE(c.maxSpin, 200.0);
    QCOMPARE(c.maxDuration, 60.0);
    QCOMPARE(c.steerAcceleration, 0.0);
    QVERIFY(!c.specialsEnabled);
    QVERIFY(MatchRules::fromJson(rules.toJson()) == rules);

    Battle battle({attackTop(), defenseTop()}, Stadium::classic(), c, 1);
    QCOMPARE(battle.spinner(0).spin, 200.0);
}

void TestBattle::launchPowerScalesSpeed()
{
    Battle battle(attackTop(), defenseTop());
    battle.launch({0.0, M_PI}, {1.5, 0.1}); // 0.1 est ramené au minimum.
    const double v0 = battle.spinner(0).velocity.length();
    const double v1 = battle.spinner(1).velocity.length();
    QVERIFY(std::abs(v0 - battle.config().launchSpeed * battle.config().launchPowerMax) < 1e-9);
    QVERIFY(std::abs(v1 - battle.config().launchSpeed * battle.config().launchPowerMin) < 1e-9);
}

void TestBattle::steeringChangesTrajectory()
{
    BattleConfig config = staticConfig();
    Battle steered({defenseTop(), defenseTop()}, Stadium::classic(), config, 1);
    Battle free({defenseTop(), defenseTop()}, Stadium::classic(), config, 1);
    steered.launch({0.0, 0.0});
    free.launch({0.0, 0.0});
    steered.setSteering(0, {0.0, -1.0}); // Vers le haut.
    run(steered, 0.5);
    run(free, 0.5);
    QVERIFY(steered.spinner(0).position.y < free.spinner(0).position.y - 0.01);
    QVERIFY(std::abs(free.spinner(0).position.y) < 1e-9);
}

void TestBattle::topsDriftTowardCenter()
{
    BattleConfig config;
    config.launchSpeed = 0.0;
    config.wobbleThreshold = 0.0;
    Battle battle({defenseTop(), defenseTop()}, Stadium::classic(), config, 1);
    battle.launch({0.0, 0.0});
    const double before = battle.spinner(0).position.length();
    run(battle, 1.0);
    QVERIFY(battle.spinner(0).position.length() < before);
}

void TestBattle::specialIsSingleUseAndCostsPartOfEndurance()
{
    Battle battle(withBolt(attackTop(), "B2"), defenseTop(), staticConfig(), 1);
    QVERIFY(!battle.canSpecial(0)); // Pas avant le lancement.
    battle.launch(0.0, M_PI);
    battle.advance(0.5);
    QVERIFY(battle.canSpecial(0)); // Disponible dès le début, sans charge.
    const double before = battle.spinner(0).spin;
    QVERIFY(battle.requestSpecial(0));
    QCOMPARE(battle.spinner(0).spin, before * (1.0 - battle.config().specialCost));
    QVERIFY(battle.spinner(0).specialUsed);
    QCOMPARE(battle.projectiles().size(), size_t(1));
    // Une seule fois par partie.
    battle.advance(3.0);
    QVERIFY(!battle.canSpecial(0));
    QVERIFY(!battle.requestSpecial(0));

    // Désactivés par les règles.
    MatchRules rules;
    rules.specials = false;
    Battle without(attackTop(), defenseTop(), rules.apply(BattleConfig{}), 1);
    without.launch(0.0, M_PI);
    without.advance(0.1);
    QVERIFY(!without.canSpecial(0));
}

void TestBattle::waveHitsTargetAndAppliesEffect_data()
{
    QTest::addColumn<QString>("bolt");
    QTest::newRow("lance") << QStringLiteral("B2");
    QTest::newRow("givre") << QStringLiteral("B3");
    QTest::newRow("distorsion") << QStringLiteral("B5");
    QTest::newRow("siphon") << QStringLiteral("B6");
}

void TestBattle::waveHitsTargetAndAppliesEffect()
{
    QFETCH(QString, bolt);
    const Top shooter = withBolt(attackTop(), bolt.toLatin1().constData());
    Battle battle({shooter, enduranceTop()}, Stadium::classic(), staticConfig(), 1);
    battle.launch({0.0, 0.0});
    battle.advance(0.01);
    const double spinBefore = battle.spinner(1).spin;
    QVERIFY(battle.requestSpecial(0));
    const double ownerAfterLaunch = battle.spinner(0).spin;
    const SpecialMove move = *SpecialMove::find(shooter.special());
    QVERIFY(move.shape == SpecialMove::Shape::Wave);

    runUntil(battle, 3.0, [&] { return battle.projectiles().empty(); });
    QVERIFY(battle.projectiles().empty()); // La vague a atteint sa cible.
    const Spinner &target = battle.spinner(1);
    if (move.damage > 0.0)
        QVERIFY(target.spin < spinBefore - 0.5);

    switch (move.effect) {
    case SpecialMove::Effect::Slow: {
        QVERIFY(target.isSlowed());
        // Le froid use la rotation : perte passive plus forte que sans gel.
        Spinner thawed = target;
        thawed.slowTimer = 0.0;
        QVERIFY(battle.passiveLossPerSecond(target) > 2.0 * battle.passiveLossPerSecond(thawed));
        break;
    }
    case SpecialMove::Effect::Teleport:
        QVERIFY(std::abs(battle.geometry().level(target.position) - move.effectStrength) < 0.1);
        break;
    case SpecialMove::Effect::Drain:
        // Après le coût du lancer, le vol rend plus que l'usure du trajet.
        QVERIFY(battle.spinner(0).spin > ownerAfterLaunch);
        break;
    default:
        break;
    }
}

void TestBattle::ringHitsEveryOpponentInRange()
{
    // 4 toupies : celle d'en face (loin) n'est pas touchée par un anneau court.
    const Top quake = withBolt(defenseTop(), "B7"); // Séisme, portée 0,5.
    BattleConfig config = staticConfig();
    // Toupie d'en face juste hors de portée, voisines (à 0,71 × la distance) dedans.
    const SpecialMove move = *SpecialMove::find(quake.special());
    config.startOffset = (move.range + config.topRadius + 0.06) / 2.0;
    Battle battle({quake, attackTop(), attackTop(), attackTop()}, Stadium::classic(), config, 1);
    battle.launch({0.0, 0.0, 0.0, 0.0});
    battle.advance(0.01);
    QVERIFY(battle.requestSpecial(0));
    runUntil(battle, 2.0, [&] { return battle.projectiles().empty(); });
    // Voisines touchées, toupie opposée hors de portée.
    QVERIFY(battle.spinner(1).isDisarmed());
    QVERIFY(battle.spinner(3).isDisarmed());
    QVERIFY(!battle.spinner(2).isDisarmed());
}

void TestBattle::disarmBlocksDash()
{
    const Top quake = withBolt(defenseTop(), "B7");
    BattleConfig config = staticConfig();
    config.startOffset = 0.2;
    Battle battle({quake, attackTop()}, Stadium::classic(), config, 1);
    battle.launch({0.0, 0.0});
    battle.advance(0.01);
    QVERIFY(battle.requestSpecial(0));
    runUntil(battle, 1.0, [&] { return battle.spinner(1).isDisarmed(); });
    QVERIFY(battle.spinner(1).isDisarmed());
    QVERIFY(!battle.canDash(1));
    QVERIFY(!battle.canSpecial(1));
}

namespace {
/// Deux toupies immobiles proches qui dashent l'une vers l'autre au même moment.
Battle duelBattle(std::uint32_t seed = 1)
{
    BattleConfig config = staticConfig();
    config.startOffset = 0.3;
    Battle battle(attackTop(), defenseTop(), config, seed);
    battle.launch(0.0, M_PI);
    battle.requestDash(0);
    battle.requestDash(1);
    runUntil(battle, 0.5, [&] { return battle.duel().active; });
    return battle;
}
} // namespace

void TestBattle::simultaneousDashesStartDuel()
{
    Battle battle = duelBattle();
    QVERIFY(battle.duel().active);
    const double spin0 = battle.spinner(0).spin;
    const Vec2 position = battle.spinner(0).position;
    QVERIFY(!battle.canDash(0));
    run(battle, 1.0);
    // Les duellistes restent figés, sans s'user, tant que personne n'appuie.
    QVERIFY(battle.duel().active);
    QCOMPARE(battle.spinner(0).spin, spin0);
    QVERIFY((battle.spinner(0).position - position).length() < 1e-9);
    QVERIFY(battle.duel().sides[0].cursor != -1.0);
}

void TestBattle::duelMissCostsOneAndHalfDashDamage()
{
    Battle battle = duelBattle();
    Spinner dasher = battle.spinner(1);
    dasher.boostTimer = 1.0;
    const double expected = battle.config().duelDamageMultiplier
                          * battle.collisionDamage(dasher, battle.spinner(0), battle.duel().approach);
    const double before = battle.spinner(0).spin;
    const double other = battle.spinner(1).spin;
    QVERIFY(battle.duelPress(0, 1.0)); // Curseur au bord : raté.
    QVERIFY(!battle.duel().active);
    QVERIFY(std::abs(before - battle.spinner(0).spin - expected) < 1e-9);
    QCOMPARE(battle.spinner(1).spin, other);
    QVERIFY(battle.spinner(0).velocity.length() > 0.0);
}

void TestBattle::duelDrawAfterFinalStreak()
{
    Battle battle = duelBattle();
    const double spins[2] = {battle.spinner(0).spin, battle.spinner(1).spin};
    int presses = 0;
    while (battle.duel().active && presses < 40) {
        battle.duelPress(0, 0.0);
        battle.duelPress(1, 0.0);
        ++presses;
    }
    QVERIFY(!battle.duel().active);
    // La zone rétrécit d'abord, puis il faut la série finale à taille minimale.
    QVERIFY(presses > battle.config().duelStreak);
    QCOMPARE(battle.spinner(0).spin, spins[0]);
    QCOMPARE(battle.spinner(1).spin, spins[1]);
    battle.advance(0.05);
    const Vec2 gap = battle.spinner(1).position - battle.spinner(0).position;
    QVERIFY((battle.spinner(1).velocity - battle.spinner(0).velocity).dot(gap) > 0.0);
}

void TestBattle::idleDuellistLoses()
{
    Battle battle = duelBattle();
    const double before = battle.spinner(1).spin;
    runUntil(battle, battle.config().duelTimeout + 1.0, [&] {
        battle.duelPress(0, 0.0); // Le joueur 0 appuie bien, le joueur 1 ne fait rien.
        return !battle.duel().active;
    });
    QVERIFY(!battle.duel().active);
    QVERIFY(battle.spinner(1).spin < before);
}

void TestBattle::harderBotsWinDuels()
{
    int hardWins = 0;
    constexpr int kDuels = 200;
    for (int k = 0; k < kDuels; ++k) {
        Battle battle = duelBattle(static_cast<std::uint32_t>(k));
        const int hardSeat = k % 2;
        BotController bots[2] = {{0, hardSeat == 0 ? Difficulty::Hard : Difficulty::Easy, 3u * k},
                                 {1, hardSeat == 1 ? Difficulty::Hard : Difficulty::Easy, 3u * k + 1}};
        const double spins[2] = {battle.spinner(0).spin, battle.spinner(1).spin};
        while (battle.duel().active) {
            for (BotController &bot : bots)
                bot.update(battle, 1.0 / 60.0);
            battle.advance(1.0 / 60.0);
        }
        const int easySeat = 1 - hardSeat;
        hardWins += battle.spinner(easySeat).spin < spins[easySeat];
    }
    qInfo("Duels gagnés par le bot difficile contre le facile : %d/%d", hardWins, kDuels);
    QVERIFY(hardWins > kDuels * 0.7);
}

void TestBattle::finalEliminationKeepsWinnerEndurance()
{
    // Un dash éjecte l'adversaire : la récompense de mêlée ne s'applique pas
    // à l'élimination qui termine le combat.
    BattleConfig config = staticConfig();
    config.startOffset = 0.3;
    Battle battle({attackTop(), enduranceTop()}, *Stadium::find(QStringLiteral("terre")), config, 1);
    battle.launch(0.0, M_PI);
    battle.advance(0.01);
    QVERIFY(battle.requestDash(0));
    double spin = battle.spinner(0).spin;
    for (int i = 0; i < 60 * 10 && !battle.isFinished(); ++i) {
        spin = battle.spinner(0).spin;
        battle.advance(1.0 / 60.0);
    }
    QVERIFY(battle.isFinished());
    QCOMPARE(battle.winner(), 0);
    QVERIFY(battle.spinner(0).spin <= spin);
}

namespace {
/// Part des victoires du niveau `strong` contre `weak` (toupies tirées au hasard).
double winRate(Difficulty strong, Difficulty weak, int fights)
{
    const PartCatalog &c = PartCatalog::instance();
    QList<Top> pool;
    for (const Part &w : c.parts(PartKind::Wheel))
        for (const Part &m : c.parts(PartKind::Middle))
            for (const Part &t : c.parts(PartKind::Tip))
                pool.append(Top(QStringLiteral("t"), w, m, t, c.parts(PartKind::Bolt).at(
                                    (w.hue + m.hue + t.hue) % c.parts(PartKind::Bolt).size())));
    std::mt19937 rng(2024);
    std::uniform_int_distribution<qsizetype> pick(0, pool.size() - 1);
    double score = 0.0;
    double totalTime = 0.0;
    for (int k = 0; k < fights; ++k) {
        const int strongSeat = k % 2;
        Battle battle({pool.at(pick(rng)), pool.at(pick(rng))}, Stadium::classic(), BattleConfig{},
                      static_cast<std::uint32_t>(k));
        std::vector<BotController> bots;
        QList<double> angles, powers;
        for (int i = 0; i < 2; ++i) {
            bots.emplace_back(i, i == strongSeat ? strong : weak, 77u * k + i);
            angles.append(bots.back().chooseLaunchAngle(battle));
            powers.append(bots.back().chooseLaunchPower(battle));
        }
        battle.launch(angles, powers);
        while (!battle.isFinished()) {
            for (BotController &bot : bots)
                bot.update(battle, 1.0 / 30.0);
            battle.advance(1.0 / 30.0);
        }
        score += battle.winner() == strongSeat ? 1.0 : (battle.winner() < 0 ? 0.5 : 0.0);
        totalTime += battle.elapsed();
    }
    qInfo("%s contre %s : %.1f %% (durée moyenne %.1f s)", qPrintable(toString(strong)),
          qPrintable(toString(weak)), 100.0 * score / fights, totalTime / fights);
    return score / fights;
}
} // namespace

void TestBattle::harderBotsWinMoreOften()
{
    // Statistique sur des combats déterministes : les niveaux doivent rester ordonnés.
    QVERIFY2(winRate(Difficulty::Normal, Difficulty::Easy, 300) > 0.6, "normal > facile");
    QVERIFY2(winRate(Difficulty::Hard, Difficulty::Easy, 300) > 0.6, "difficile > facile");
    QVERIFY2(winRate(Difficulty::Hard, Difficulty::Normal, 300) > 0.58, "difficile > normal");
}

void TestBattle::botKeepsSpecialForLater()
{
    // En pleine forme, le coup spécial coûterait 50 points : le bot difficile attend.
    Battle battle(withBolt(attackTop(), "B2"), enduranceTop(), staticConfig(), 1);
    battle.launch(0.0, M_PI);
    battle.advance(0.2);
    BotController bot(0, Difficulty::Hard, 1);
    QVERIFY(battle.canSpecial(0));
    QVERIFY(bot.specialValue(battle) < 0.0);
}

QTEST_GUILESS_MAIN(TestBattle)
#include "tst_battle.moc"
#include <QtMath>
