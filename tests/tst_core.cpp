#include "core/PartCatalog.h"
#include "core/Stats.h"
#include "core/Top.h"
#include "core/TopRepository.h"

#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace toupie;

// Remarque : les énumérations de toupie ont une surcharge toString() renvoyant
// un QString, incompatible avec QCOMPARE ; on les compare donc avec QVERIFY.

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void classify_data();
    void classify();
    void topStatsAreSumOfParts();
    void wheelDefinesSpin();
    void catalogPartsRespectBudgets();
    void catalogIdsAreUnique();
    void presetsCoverEveryType();
    void maxStatValueMatchesCatalog();
    void jsonRoundTrip();
    void jsonWithUnknownPartIsRejected();
    void repositorySaveAndLoad();
    void repositoryUpsertReplacesByName();
};

void TestCore::classify_data()
{
    QTest::addColumn<int>("attack");
    QTest::addColumn<int>("defense");
    QTest::addColumn<int>("endurance");
    QTest::addColumn<TopType>("expected");

    QTest::newRow("exemple de l'énoncé") << 6 << 12 << 5 << TopType::Defense;
    QTest::newRow("attaque") << 14 << 4 << 5 << TopType::Attack;
    QTest::newRow("endurance") << 4 << 4 << 15 << TopType::Endurance;
    QTest::newRow("égalité au sommet") << 8 << 8 << 7 << TopType::Balance;
    QTest::newRow("égalité totale") << 7 << 7 << 7 << TopType::Balance;
    QTest::newRow("égalité attaque/endurance") << 9 << 5 << 9 << TopType::Balance;
}

void TestCore::classify()
{
    QFETCH(int, attack);
    QFETCH(int, defense);
    QFETCH(int, endurance);
    QFETCH(TopType, expected);
    QVERIFY(toupie::classify(Stats{attack, defense, endurance}) == expected);
}

void TestCore::topStatsAreSumOfParts()
{
    const PartCatalog &catalog = PartCatalog::instance();
    // Exemple de l'énoncé : RM1 (3/5/2) + PI1 (2/3/1) + P1 (1/4/2) = 6/12/5.
    const Top top(QStringLiteral("Exemple"), *catalog.find("RM1"), *catalog.find("PI1"),
                  *catalog.find("P1"));
    QCOMPARE(top.stats(), (Stats{6, 12, 5}));
    QVERIFY(top.type() == TopType::Defense);
    QVERIFY(top.isValid());
}

void TestCore::wheelDefinesSpin()
{
    const PartCatalog &catalog = PartCatalog::instance();
    for (const Part &wheel : catalog.parts(PartKind::Wheel)) {
        const Top top(QStringLiteral("t"), wheel, catalog.parts(PartKind::Middle).first(),
                      catalog.parts(PartKind::Tip).first());
        QVERIFY(top.spin() == wheel.spin);
    }
}

void TestCore::catalogPartsRespectBudgets()
{
    for (const Part &part : PartCatalog::instance().parts()) {
        QVERIFY2(part.stats.total() == PartCatalog::budget(part.kind), qPrintable(part.name));
        QVERIFY(part.stats.attack >= 1 && part.stats.defense >= 1 && part.stats.endurance >= 1);
    }
}

void TestCore::catalogIdsAreUnique()
{
    QSet<QString> ids;
    for (const Part &part : PartCatalog::instance().parts()) {
        QVERIFY2(!ids.contains(part.id), qPrintable(part.id));
        ids.insert(part.id);
    }
    for (PartKind kind : {PartKind::Wheel, PartKind::Middle, PartKind::Tip})
        QVERIFY(PartCatalog::instance().parts(kind).size() >= 2);
}

void TestCore::presetsCoverEveryType()
{
    QSet<TopType> types;
    for (const Top &top : PartCatalog::instance().presets()) {
        QVERIFY(top.isValid());
        QCOMPARE(top.stats().total(), 23);
        types.insert(top.type());
    }
    QCOMPARE(types.size(), 4);
}

void TestCore::maxStatValueMatchesCatalog()
{
    // Nébuleuse (7) + Anneau (4) + Aiguille (5) en endurance.
    QCOMPARE(PartCatalog::instance().maxStatValue(), 16);
}

void TestCore::jsonRoundTrip()
{
    const PartCatalog &catalog = PartCatalog::instance();
    const Top original = catalog.presets().first();
    const auto restored = Top::fromJson(original.toJson(), catalog);
    QVERIFY(restored.has_value());
    QCOMPARE(restored->name(), original.name());
    QCOMPARE(restored->wheel().id, original.wheel().id);
    QCOMPARE(restored->middle().id, original.middle().id);
    QCOMPARE(restored->tip().id, original.tip().id);
}

void TestCore::jsonWithUnknownPartIsRejected()
{
    QJsonObject json = PartCatalog::instance().presets().first().toJson();
    json.insert(QStringLiteral("tip"), QStringLiteral("inexistant"));
    QVERIFY(!Top::fromJson(json, PartCatalog::instance()).has_value());
}

void TestCore::repositorySaveAndLoad()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("sub/tops.json"));
    const PartCatalog &catalog = PartCatalog::instance();

    TopRepository repo(catalog, path);
    QVERIFY(repo.load()); // Fichier absent : pas une erreur.
    Top top = catalog.presets().at(1);
    top.setName(QStringLiteral("Ma toupie"));
    repo.upsert(top);
    QVERIFY(repo.save());

    TopRepository other(catalog, path);
    QVERIFY(other.load());
    QCOMPARE(other.customTops().size(), 1);
    QCOMPARE(other.customTops().first().name(), QStringLiteral("Ma toupie"));
    QCOMPARE(other.customTops().first().stats(), top.stats());
    QCOMPARE(other.allTops().size(), catalog.presets().size() + 1);
}

void TestCore::repositoryUpsertReplacesByName()
{
    QTemporaryDir dir;
    const PartCatalog &catalog = PartCatalog::instance();
    TopRepository repo(catalog, dir.filePath(QStringLiteral("tops.json")));

    Top a = catalog.presets().at(0);
    a.setName(QStringLiteral("Alpha"));
    Top b = catalog.presets().at(2);
    b.setName(QStringLiteral("ALPHA"));

    repo.upsert(a);
    repo.upsert(b);
    QCOMPARE(repo.customTops().size(), 1);
    QVERIFY(repo.customTops().first().type() == b.type());

    QVERIFY(repo.remove(QStringLiteral("alpha")));
    QVERIFY(repo.customTops().isEmpty());
    QVERIFY(!repo.remove(QStringLiteral("alpha")));
}

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
