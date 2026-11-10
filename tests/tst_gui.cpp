// Tests d'intégration de l'interface (plateforme « offscreen ») : navigation,
// partie solo complète, atelier et lobby réseau entre deux fenêtres.

#include "core/PartCatalog.h"
#include "core/TopRepository.h"
#include "ui/ArenaWidget.h"
#include "ui/BattlePage.h"
#include "ui/BuilderPage.h"
#include "ui/Controls.h"
#include "ui/HelpPage.h"
#include "ui/Language.h"
#include "ui/SettingsPage.h"
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
#include "ui/TutorialSession.h"

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
    void soloMeleeAgainstSeveralBots();
    void builderSavesTopWithBolt();
    void lanLobbyStartsMatchWithHostRules();
    void soloTournamentOpensMatch();
    void tournamentParticipantsFollowFormat();
    void controlsAreRebindableAndSaved();
    void reboundKeyIsUsedInBattle();
    void tutorialWalksThroughEveryAction();
    void englishTranslationKeepsGameName();
};

void TestGui::initTestCase()
{
    // Données isolées : les tests n'écrivent jamais dans les toupies du joueur.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("ToupieArenaTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ToupieArenaTests"));
    QFile::remove(TopRepository::defaultFilePath());
    Controls::instance().reset(); // Touches par défaut, quoi qu'ait laissé un essai précédent.
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
    emit window.findChild<SetupPage *>()->fightRequested(presets.at(0), {presets.at(2)},
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
            QVERIFY(battle->session()->state().spinners[0].spin
                    <= before * (1.0 - battle->session()->state().config.specialCost) + 1.0);
    }
    QTRY_COMPARE_WITH_TIMEOUT(battle->session()->state().phase, MatchPhase::Finished, 12000);

    // Revanche : la même session repart en phase de visée.
    QTest::keyClick(arena, Qt::Key_R);
    QCOMPARE(battle->session()->state().phase, MatchPhase::Aiming);
}

void TestGui::soloMeleeAgainstSeveralBots()
{
    MainWindow window;
    window.show();
    auto *setup = window.findChild<SetupPage *>();
    auto *battle = window.findChild<BattlePage *>();
    auto *arena = window.findChild<ArenaWidget *>();

    // Trois bots : mêlée à quatre toupies, numérotées.
    emit window.findChild<MenuPage *>()->playRequested();
    setup->findChild<QPushButton *>(QStringLiteral("bots3"))->click();
    QCOMPARE(setup->botCount(), 3);
    buttonStartingWith(setup, QStringLiteral("Combattre"))->click();
    QCOMPARE(currentPage(window), battle);
    QCOMPARE(battle->session()->state().playerCount(), 4);
    QCOMPARE(battle->session()->state().names.value(3), QStringLiteral("Bot 3"));
    // Les bots visent d'office : le lancer du joueur suffit à démarrer.
    arena->setFocus();
    QTest::keyClick(arena, Qt::Key_Return);
    QTRY_COMPARE_WITH_TIMEOUT(battle->session()->state().phase, MatchPhase::Fighting, 5000);
    QTest::qWait(500);
    QVERIFY(battle->session()->state().spinners[3].status != Spinner::Status::Waiting);

    // Deux bots : trois toupies.
    emit battle->setupRequested();
    QCOMPARE(currentPage(window), setup);
    setup->setBotCount(2);
    buttonStartingWith(setup, QStringLiteral("Combattre"))->click();
    QCOMPARE(battle->session()->state().playerCount(), 3);
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
    const QList<PartSlot *> slotCards = builder->findChildren<PartSlot *>();
    QCOMPARE(slotCards.size(), 4); // Boulon, roue, intermédiaire, pointe.
    QVERIFY(slotCards.first()->kind() == PartKind::Bolt);
    slotCards.first()->setIndex(3);
    slotCards.at(1)->setIndex(-1); // En boucle : dernière roue.
    QCOMPARE(slotCards.at(1)->part().id, PartCatalog::instance().parts(PartKind::Wheel).last().id);
    buttonStartingWith(builder, QStringLiteral("Enregistrer"))->click();

    TopRepository repository(PartCatalog::instance());
    QVERIFY(repository.load());
    QVERIFY(repository.contains(QStringLiteral("Test CI")));
    const Top &saved = repository.customTops().first();
    QCOMPARE(saved.bolt().id, PartCatalog::instance().parts(PartKind::Bolt).at(3).id);
    QCOMPARE(saved.wheel().id, PartCatalog::instance().parts(PartKind::Wheel).last().id);
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
    buttonStartingWith(setup, QStringLiteral("Tournoi solo"))->click();
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

void TestGui::controlsAreRebindableAndSaved()
{
    Controls &controls = Controls::instance();
    QVERIFY(controls.triggers(Qt::Key_Space, Action::Dash));
    QVERIFY(controls.triggers(Controls::mouseCode(Qt::LeftButton), Action::Dash));
    // Une touche déjà utilisée est retirée de l'action précédente.
    const auto previous = controls.bind(Action::Special, 1, Qt::Key_Space);
    QVERIFY(previous && *previous == Action::Dash);
    QVERIFY(!controls.triggers(Qt::Key_Space, Action::Dash));
    QVERIFY(controls.triggers(Qt::Key_Space, Action::Special));
    QVERIFY(!controls.bind(Action::Dash, 0, Controls::mouseCode(Qt::RightButton)));
    QVERIFY(!controls.bind(Action::Dash, 1, Qt::Key_Escape)); // Réservée.
    QCOMPARE(controls.binding(Action::Dash, 1), Controls::mouseCode(Qt::LeftButton));

    const Controls reloaded; // Relu depuis les réglages.
    QCOMPARE(reloaded.binding(Action::Special, 1), int(Qt::Key_Space));
    QCOMPARE(reloaded.binding(Action::Dash, 0), Controls::mouseCode(Qt::RightButton));
    QVERIFY(reloaded.helpHtml().contains(QStringLiteral("Clic droit")));
    controls.reset();
    QVERIFY(controls.triggers(Qt::Key_Space, Action::Dash));
}

void TestGui::reboundKeyIsUsedInBattle()
{
    Controls::instance().bind(Action::Launch, 0, Qt::Key_L);
    MainWindow window;
    window.show();
    emit window.findChild<MenuPage *>()->helpRequested();
    QVERIFY(qobject_cast<HelpPage *>(currentPage(window)));
    emit window.findChild<MenuPage *>()->settingsRequested();
    QVERIFY(qobject_cast<SettingsPage *>(currentPage(window)));

    const QList<Top> presets = PartCatalog::instance().presets();
    emit window.findChild<SetupPage *>()->fightRequested(presets.at(0), {presets.at(1)},
                                                         Difficulty::Easy, QString(), {});
    auto *battle = window.findChild<BattlePage *>();
    auto *arena = window.findChild<ArenaWidget *>();
    arena->setFocus();
    QTest::keyClick(arena, Qt::Key_Return); // Ancienne touche : plus associée au lancer.
    QCOMPARE(battle->session()->state().phase, MatchPhase::Aiming);
    QTest::keyClick(arena, Qt::Key_L);
    QCOMPARE(battle->session()->state().phase, MatchPhase::Countdown);
    Controls::instance().reset();
}

void TestGui::tutorialWalksThroughEveryAction()
{
    using Step = TutorialSession::Step;
    MainWindow window;
    window.show();
    emit window.findChild<MenuPage *>()->tutorialRequested();
    auto *battle = window.findChild<BattlePage *>();
    QCOMPARE(currentPage(window), battle);
    QCOMPARE(battle->mode(), BattlePage::Mode::Tutorial);
    auto *tutorial = qobject_cast<TutorialSession *>(battle->session());
    QVERIFY(tutorial);
    auto *arena = window.findChild<ArenaWidget *>();
    arena->setFocus();
    auto waitPause = [&](Step step) {
        QTRY_VERIFY_WITH_TIMEOUT(tutorial->isPaused() && tutorial->step() == step, 20000);
        QVERIFY(!tutorial->bubble().isEmpty());
    };

    // Visée : seules les touches présentées font avancer ; un clic valide la visée
    // sans lancer la toupie.
    QTest::keyClick(arena, Qt::Key_E);
    QCOMPARE(tutorial->step(), Step::Aim);
    QTest::mouseClick(arena, Qt::LeftButton, {}, arena->rect().center());
    QCOMPARE(tutorial->state().phase, MatchPhase::Aiming);
    QCOMPARE(tutorial->step(), Step::Power);
    QTest::keyClick(arena, Qt::Key_Up);
    QCOMPARE(tutorial->step(), Step::Launch);
    QTest::keyClick(arena, Qt::Key_Return);
    QCOMPARE(tutorial->state().phase, MatchPhase::Countdown);

    // Pilotage : le jeu est figé tant que la touche n'est pas pressée.
    waitPause(Step::Steer);
    const double elapsed = tutorial->state().elapsed;
    QTest::qWait(300);
    QCOMPARE(tutorial->state().elapsed, elapsed);
    QTest::keyClick(arena, Qt::Key_E); // Pas la touche présentée : ignorée.
    QVERIFY(tutorial->isPaused());
    QTest::keyPress(arena, Qt::Key_Right);
    QVERIFY(!tutorial->isPaused());
    QTest::qWait(200);
    QTest::keyRelease(arena, Qt::Key_Right);

    waitPause(Step::Dash);
    QTest::keyClick(arena, Qt::Key_Space);
    QTRY_VERIFY(tutorial->state().spinners[0].dashCooldown > 0.0 || tutorial->state().duel.active);
    waitPause(Step::Special);
    QTest::keyClick(arena, Qt::Key_E);
    QTRY_VERIFY(tutorial->state().spinners[0].specialUsed);
    waitPause(Step::Duel);
    QTest::keyClick(arena, Qt::Key_Space);
    QCOMPARE(tutorial->step(), Step::AfterDuel);

    // Duel guidé : explication, puis un raté n'est pas compté.
    waitPause(Step::DuelIntro);
    QVERIFY(tutorial->state().duel.active);
    QTest::keyClick(arena, Qt::Key_Space);
    QCOMPARE(tutorial->step(), Step::DuelPlay);
    auto cursorInGreen = [&] {
        const DuelSide &side = tutorial->state().duel.sides[tutorial->state().duel.sideOf(0)];
        return std::abs(side.cursor) <= side.zone * 0.5;
    };
    QTRY_VERIFY(!cursorInGreen());
    QTest::keyClick(arena, Qt::Key_Space); // Dans le rouge.
    QCOMPARE(tutorial->step(), Step::DuelMiss);
    QVERIFY(tutorial->isPaused());
    QVERIFY(tutorial->state().duel.active); // Le raté ne fait pas perdre le duel.
    QTest::keyClick(arena, Qt::Key_Space);
    QCOMPARE(tutorial->step(), Step::DuelPlay);
    for (int i = 0; i < 3; ++i) {
        QTRY_VERIFY(tutorial->step() != Step::DuelPlay || cursorInGreen());
        if (tutorial->step() == Step::DuelPlay)
            QTest::keyClick(arena, Qt::Key_Space);
        QTest::qWait(150);
    }

    // Duel gagné : la toupie d'entraînement est éjectée, le joueur gagne.
    QTRY_COMPARE_WITH_TIMEOUT(tutorial->state().phase, MatchPhase::Finished, 5000);
    QCOMPARE(tutorial->state().winner, 0);
    QVERIFY(tutorial->state().endReason == Battle::EndReason::RingOut);
    QCOMPARE(tutorial->step(), Step::Done);
    QVERIFY(!tutorial->bubble().isEmpty());
}

void TestGui::englishTranslationKeepsGameName()
{
    Language::setCurrent(QStringLiteral("en"));
    Language::install(*qApp);
    QCOMPARE(Language::current(), QStringLiteral("en"));
    QCOMPARE(toString(TopType::Endurance), QStringLiteral("Stamina"));
    {
        MainWindow window;
        QCOMPARE(window.windowTitle(), QStringLiteral("Top Arena")); // Jamais traduit.
        QVERIFY(buttonStartingWith(window.findChild<MenuPage *>(), QStringLiteral("Play vs bots")));
    }
    // Retour au français : les traductions installées sont retirées.
    Language::setCurrent(QStringLiteral("fr"));
    Language::install(*qApp);
    QCOMPARE(toString(TopType::Endurance), QStringLiteral("Endurance"));
}

QTEST_MAIN(TestGui)
#include "tst_gui.moc"
