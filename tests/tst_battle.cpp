#include "core/PartCatalog.h"
#include "game/Battle.h"
#include "game/BotController.h"

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

QTEST_GUILESS_MAIN(TestBattle)
#include "tst_battle.moc"
