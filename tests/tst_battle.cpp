#include "core/PartCatalog.h"
#include "game/Battle.h"
#include "game/BotController.h"
#include "game/MatchRules.h"

#include <QSet>
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
    void specialRequiresFullGauge();
    void waveHitsTargetAndAppliesEffect_data();
    void waveHitsTargetAndAppliesEffect();
    void ringHitsEveryOpponentInRange();
    void disarmBlocksDash();
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
    c.specialChargeRate = 1e6; // Jauge pleine dès le premier pas.
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
    rules.specialCharge = 0.0;
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

void TestBattle::specialRequiresFullGauge()
{
    Battle battle(attackTop(), defenseTop());
    battle.launch(0.0, M_PI);
    QVERIFY(!battle.canSpecial(0)); // Jauge vide au départ.
    run(battle, 100.0 / battle.config().specialChargeRate + 0.2);
    if (!battle.isFinished()) {
        QVERIFY(battle.canSpecial(0));
        QVERIFY(battle.requestSpecial(0));
        QCOMPARE(battle.spinner(0).special, 0.0);
        QCOMPARE(battle.projectiles().size(), size_t(1));
    }
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
    Battle battle({shooter, defenseTop()}, Stadium::classic(), staticConfig(), 1);
    battle.launch({0.0, 0.0});
    battle.advance(0.01);
    const double spinBefore = battle.spinner(1).spin;
    const double ownerBefore = battle.spinner(0).spin;
    QVERIFY(battle.requestSpecial(0));
    const SpecialMove move = *SpecialMove::find(shooter.special());
    QVERIFY(move.shape == SpecialMove::Shape::Wave);

    runUntil(battle, 3.0, [&] { return battle.projectiles().empty(); });
    QVERIFY(battle.projectiles().empty()); // La vague a atteint sa cible.
    const Spinner &target = battle.spinner(1);
    if (move.damage > 0.0)
        QVERIFY(target.spin < spinBefore - 0.5);

    switch (move.effect) {
    case SpecialMove::Effect::Slow:
        QVERIFY(target.isSlowed());
        break;
    case SpecialMove::Effect::Teleport:
        QVERIFY(std::abs(battle.geometry().level(target.position) - move.effectStrength) < 0.1);
        break;
    case SpecialMove::Effect::Drain:
        // La toupie lanceuse a récupéré de l'endurance malgré l'usure.
        QVERIFY(battle.spinner(0).spin > ownerBefore - 1.0);
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
    config.startOffset = 0.3;
    Battle battle({quake, attackTop(), attackTop(), attackTop()}, Stadium::classic(), config, 1);
    battle.launch({0.0, 0.0, 0.0, 0.0});
    battle.advance(0.01);
    QVERIFY(battle.requestSpecial(0));
    runUntil(battle, 2.0, [&] { return battle.projectiles().empty(); });
    // Voisins à 0,42 (touchés), toupie opposée à 0,6 (hors portée).
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

QTEST_GUILESS_MAIN(TestBattle)
#include "tst_battle.moc"
#include <QtMath>
