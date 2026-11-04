// Tests d'intégration de l'interface (plateforme « offscreen ») : navigation,
// partie solo complète, atelier et lobby réseau entre deux fenêtres.

#include "core/PartCatalog.h"
#include "core/TopRepository.h"
#include "ui/ArenaWidget.h"
#include "ui/BattlePage.h"
#include "ui/BuilderPage.h"
#include "ui/LanMenuPage.h"
#include "ui/LobbyPage.h"
#include "ui/MainWindow.h"
#include "ui/MenuPage.h"
#include "ui/RulesWidget.h"
#include "ui/SetupPage.h"
#include "ui/Theme.h"
#include "ui/TournamentPage.h"
#include "ui/TournamentSettingsWidget.h"
#include "ui/TournamentSetupPage.h"

#include <QSpinBox>

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTest>

using namespace toupie;

namespace {
QWidget *currentPage(MainWindow &window)
{
    return window.findChild<QStackedWidget *>()->currentWidget();
}

QPushButton *buttonStartingWith(QWidget *parent, const QString &text)
{
    for (QPushButton *button : parent->findChildren<QPushButton *>()) {
        if (button->text().startsWith(text))
            return button;
    }
    return nullptr;
}
} // namespace

class TestGui : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void menuNavigation();
    void soloMatchRunsToTheEnd();
    void builderSavesTopWithBolt();
    void lanLobbyStartsMatchWithHostRules();
    void soloTournamentOpensMatch();
    void tournamentParticipantsFollowFormat();
};

void TestGui::initTestCase()
{
    // Données isolées : les tests n'écrivent jamais dans les toupies du joueur.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("ToupieArenaTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ToupieArenaTests"));
    QFile::remove(TopRepository::defaultFilePath());
    Theme::apply(*qApp);
}

void TestGui::menuNavigation()
{
    MainWindow window;
    window.show();
    auto *menu = window.findChild<MenuPage *>();
    QCOMPARE(currentPage(window), menu);

    emit menu->playRequested();
    QVERIFY(qobject_cast<SetupPage *>(currentPage(window)));
    emit window.findChild<SetupPage *>()->backRequested();
    QCOMPARE(currentPage(window), menu);

    emit menu->builderRequested();
    QVERIFY(qobject_cast<BuilderPage *>(currentPage(window)));
    emit window.findChild<BuilderPage *>()->backRequested();

    emit menu->lanRequested();
    QVERIFY(qobject_cast<LanMenuPage *>(currentPage(window)));
    emit window.findChild<LanMenuPage *>()->backRequested();
    QCOMPARE(currentPage(window), menu);
}

void TestGui::soloMatchRunsToTheEnd()
{
    MainWindow window;
    window.show();
    const QList<Top> presets = PartCatalog::instance().presets();
    MatchRules rules;
    rules.timeLimit = 8;      // Partie courte.
    emit window.findChild<SetupPage *>()->fightRequested(presets.at(0), presets.at(2),
                                                         Difficulty::Hard,
                                                         QStringLiteral("glace"), rules);
    auto *battle = window.findChild<BattlePage *>();
    QCOMPARE(currentPage(window), battle);
    QVERIFY(battle->session());
    QCOMPARE(battle->session()->state().stadium.id, QStringLiteral("glace"));
    QCOMPARE(battle->session()->state().config.maxDuration, 8.0);

    auto *arena = window.findChild<ArenaWidget *>();
    arena->setFocus();
    QTest::keyClick(arena, Qt::Key_Up); // Force de lancer +5 %.
    QTest::keyClick(arena, Qt::Key_Return);
    QCOMPARE(battle->session()->state().phase, MatchPhase::Countdown);
    QTRY_COMPARE_WITH_TIMEOUT(battle->session()->state().phase, MatchPhase::Fighting, 5000);

    // Pilotage et coup spécial au clavier.
    QTest::keyPress(arena, Qt::Key_Left);
    QTest::qWait(300);
    QTest::keyRelease(arena, Qt::Key_Left);
    if (battle->session()->state().phase == MatchPhase::Fighting
        && battle->session()->state().spinners[0].isActive()) {
        const double before = battle->session()->state().spinners[0].spin;
        QTest::keyClick(arena, Qt::Key_E); // Coup spécial (une fois par partie).
        QTRY_VERIFY_WITH_TIMEOUT(battle->session()->state().phase == MatchPhase::Finished
                                     || battle->session()->state().spinners[0].specialUsed,
                                 2000);
        if (battle->session()->state().spinners[0].specialUsed)
            QVERIFY(battle->session()->state().spinners[0].spin <= before * 0.5 + 1.0);
    }
    QTRY_COMPARE_WITH_TIMEOUT(battle->session()->state().phase, MatchPhase::Finished, 12000);

    // Revanche : la même session repart en phase de visée.
    QTest::keyClick(arena, Qt::Key_R);
    QCOMPARE(battle->session()->state().phase, MatchPhase::Aiming);
}

void TestGui::builderSavesTopWithBolt()
{
    MainWindow window;
    window.show();
    emit window.findChild<MenuPage *>()->builderRequested();
    auto *builder = window.findChild<BuilderPage *>();
    auto *name = builder->findChild<QLineEdit *>();
    QVERIFY(name);
    name->setText(QStringLiteral("Test CI"));
    const QList<QComboBox *> combos = builder->findChildren<QComboBox *>();
    QCOMPARE(combos.size(), 4); // Boulon, roue, intermédiaire, pointe.
    combos.first()->setCurrentIndex(3);
    buttonStartingWith(builder, QStringLiteral("Enregistrer"))->click();

    TopRepository repository(PartCatalog::instance());
    QVERIFY(repository.load());
    QVERIFY(repository.contains(QStringLiteral("Test CI")));
    const Top &saved = repository.customTops().first();
    QCOMPARE(saved.bolt().id, PartCatalog::instance().parts(PartKind::Bolt).at(3).id);
}

void TestGui::lanLobbyStartsMatchWithHostRules()
{
    MainWindow host;
    MainWindow guest;
    host.show();
    guest.show();

    emit host.findChild<MenuPage *>()->lanRequested();
    net::LobbyServer::Settings settings{QStringLiteral("CI"), QStringLiteral("Hôte"), 2,
                                        QStringLiteral("sable"), std::nullopt};
    emit host.findChild<LanMenuPage *>()->hostRequested(settings);
    auto *hostLobby = host.findChild<LobbyPage *>();
    QCOMPARE(currentPage(host), hostLobby);
    QVERIFY(hostLobby->hostPort() != 0);

    // L'hôte personnalise la partie.
    MatchRules rules;
    rules.speed = 1.5;
    rules.maxEndurance = 150;
    auto *hostRules = hostLobby->findChild<RulesWidget *>();
    hostRules->setRules(rules);
    emit hostRules->rulesChanged(rules);

    emit guest.findChild<MenuPage *>()->lanRequested();
    emit guest.findChild<LanMenuPage *>()->joinRequested(QStringLiteral("127.0.0.1"),
                                                         hostLobby->hostPort());
    auto *guestLobby = guest.findChild<LobbyPage *>();
    QTRY_COMPARE_WITH_TIMEOUT(currentPage(guest), guestLobby, 5000);
    QTRY_VERIFY(guestLobby->findChild<RulesWidget *>()->rules() == rules);

    buttonStartingWith(guestLobby, QStringLiteral("Je suis"))->click();
    QPushButton *start = buttonStartingWith(hostLobby, QStringLiteral("Lancer"));
    QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(), 5000);
    start->click();

    auto *hostBattle = host.findChild<BattlePage *>();
    auto *guestBattle = guest.findChild<BattlePage *>();
    QTRY_COMPARE_WITH_TIMEOUT(currentPage(guest), guestBattle, 5000);
    QCOMPARE(currentPage(host), hostBattle);
    QCOMPARE(hostBattle->mode(), BattlePage::Mode::LanHost);
    QCOMPARE(guestBattle->mode(), BattlePage::Mode::LanClient);
    QCOMPARE(guestBattle->session()->state().config.maxSpin, 150.0);
    QCOMPARE(guestBattle->session()->state().stadium.id, QStringLiteral("sable"));

    // Les deux joueurs lancent : le combat démarre chez les deux.
    QTest::keyClick(host.findChild<ArenaWidget *>(), Qt::Key_Return);
    QTest::keyClick(guest.findChild<ArenaWidget *>(), Qt::Key_Return);
    QTRY_COMPARE_WITH_TIMEOUT(guestBattle->session()->state().phase, MatchPhase::Fighting, 6000);
}

void TestGui::soloTournamentOpensMatch()
{
    MainWindow window;
    window.show();
    emit window.findChild<MenuPage *>()->tournamentRequested();
    auto *setup = qobject_cast<TournamentSetupPage *>(currentPage(window));
    QVERIFY(setup);
    buttonStartingWith(setup, QStringLiteral("🏆  Tournoi solo"))->click();
    buttonStartingWith(setup, QStringLiteral("Lancer le tournoi"))->click();
    auto *hub = qobject_cast<TournamentPage *>(currentPage(window));
    QVERIFY(hub);

    // Prêt : la première manche du match du joueur s'ouvre dans l'arène.
    buttonStartingWith(hub, QStringLiteral("Prêt"))->click();
    auto *battle = qobject_cast<BattlePage *>(currentPage(window));
    QVERIFY(battle);
    QCOMPARE(battle->mode(), BattlePage::Mode::Tournament);
    QVERIFY(battle->session() && battle->session()->state().series.has_value());
    QVERIFY(buttonStartingWith(battle, QStringLiteral("Abandonner")));
}

void TestGui::tournamentParticipantsFollowFormat()
{
    TournamentSettingsWidget widget;
    widget.setHumans(2);
    auto *count = widget.findChild<QSpinBox *>();
    auto *format = widget.findChildren<QComboBox *>().first();
    count->setValue(6); // Ligue : saisie libre.
    QCOMPARE(widget.settings().participants, 6);
    QCOMPARE(widget.settings().bots.size(), 4);
    format->setCurrentIndex(format->findData(static_cast<int>(TournamentFormat::Knockout)));
    QCOMPARE(widget.settings().participants, 8); // Tableau complet.
    QVERIFY(count->isHidden());
    format->setCurrentIndex(format->findData(static_cast<int>(TournamentFormat::League)));
    QCOMPARE(count->value(), 8);
    QVERIFY(!count->isHidden());
}

QTEST_MAIN(TestGui)
#include "tst_gui.moc"
