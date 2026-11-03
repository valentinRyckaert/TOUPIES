#include "core/PartCatalog.h"
#include "game/Battle.h"
#include "game/BotController.h"

#include <QTest>

#include <cmath>

using namespace toupie;

namespace {

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
    battle.advance(0.5);
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
        while (!battle.isFinished() && frames < 60 * 200) {
            a.update(battle, 1.0 / 60.0);
            b.update(battle, 1.0 / 60.0);
            battle.advance(1.0 / 60.0);
            ++frames;
        }
        QVERIFY(battle.isFinished());
        QVERIFY(battle.elapsed() <= battle.config().maxDuration + 0.1);
    }
}

QTEST_GUILESS_MAIN(TestBattle)
#include "tst_battle.moc"
