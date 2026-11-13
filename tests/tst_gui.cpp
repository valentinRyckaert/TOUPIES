// Tests d'intégration de l'interface (plateforme « offscreen ») : navigation,
// partie solo complète, atelier et lobby réseau entre deux fenêtres.

#include "core/PartCatalog.h"
#include "core/TopRepository.h"
#include "game/Tournament.h"
#include "ui/ArenaWidget.h"
#include "ui/Audio.h"
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
#include "ui/MovesBoard.h"
#include "ui/RulesWidget.h"
#include "ui/SetupPage.h"
#include "ui/Theme.h"
#include "ui/TournamentPage.h"
#include "ui/TournamentSettingsWidget.h"
#include "ui/TournamentSetupPage.h"
#include "ui/TutorialSession.h"

#include <QSlider>
#include <QSpinBox>

#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QSettings>
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
    void soloTournamentIsSavedAndResumed();
    void tournamentParticipantsFollowFormat();
    void controlsAreRebindableAndSaved();
    void duelModeIsChosenInSettings();
    void rulesOverrideTheDefaultDuelMode();
    void reboundKeyIsUsedInBattle();
    void tutorialWalksThroughEveryAction();
    void englishTranslationKeepsGameName();
    void soundsExistAndFollowTheBattle();
    void soundSettingsAreSaved();
};

void TestGui::initTestCase()
{
    // Données isolées : les tests n'écrivent jamais dans les toupies du joueur.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("ToupieArenaTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ToupieArenaTests"));
    QFile::remove(TopRepository::defaultFilePath());
    Controls::instance().reset(); // Touches par défaut, quoi qu'ait laissé un essai précédent.
    for (int c = 0; c < Audio::kChannels; ++c) // Tests silencieux.
        Audio::instance().setEnabled(static_cast<Audio::Channel>(c), false);
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

    // Bouton d'info du coup spécial : la planche des sept coups s'ouvre.
    QPushButton *info = nullptr;
    for (QPushButton *button : builder->findChildren<QPushButton *>()) {
        if (button->property("glyph").toString() == QLatin1String("info"))
            info = button;
    }
    QVERIFY(info);
    info->click();
    auto *moves = qobject_cast<MovesPage *>(currentPage(window));
    QVERIFY(moves);
    QVERIFY(moves->findChild<MovesBoard *>());
    emit moves->backRequested(); // Retour à l'atelier, toupie en cours conservée.
    QCOMPARE(currentPage(window), builder);
    QCOMPARE(name->text(), QStringLiteral("Test CI"));
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
    QVERIFY(buttonStartingWith(hub, QStringLiteral("Quitter"))); // Quitte (et sauvegarde), n'abandonne pas.

    // Prêt : la première manche du match du joueur s'ouvre dans l'arène.
    buttonStartingWith(hub, QStringLiteral("Prêt"))->click();
    auto *battle = qobject_cast<BattlePage *>(currentPage(window));
    QVERIFY(battle);
    QCOMPARE(battle->mode(), BattlePage::Mode::Tournament);
    QVERIFY(battle->session() && battle->session()->state().series.has_value());
    QVERIFY(buttonStartingWith(battle, QStringLiteral("Abandonner")));
}

void TestGui::soloTournamentIsSavedAndResumed()
{
    SavedTournament::remove();
    {
        MainWindow window;
        window.show();
        emit window.findChild<MenuPage *>()->tournamentRequested();
        auto *setup = qobject_cast<TournamentSetupPage *>(currentPage(window));
        QVERIFY(!buttonStartingWith(setup, QStringLiteral("Reprendre"))->isVisible());
        buttonStartingWith(setup, QStringLiteral("Tournoi solo"))->click();
        buttonStartingWith(setup, QStringLiteral("Lancer le tournoi"))->click();
        QVERIFY(qobject_cast<TournamentPage *>(currentPage(window)));
        window.close(); // Fermer le jeu en plein tournoi le sauvegarde.
    }
    QVERIFY(SavedTournament::exists());

    MainWindow window;
    window.show();
    emit window.findChild<MenuPage *>()->tournamentRequested();
    auto *setup = qobject_cast<TournamentSetupPage *>(currentPage(window));
    QPushButton *resume = buttonStartingWith(setup, QStringLiteral("Reprendre"));
    QVERIFY(resume && resume->isVisible());
    resume->click();
    QVERIFY(qobject_cast<TournamentPage *>(currentPage(window)));
    QVERIFY(!SavedTournament::exists()); // Repris : la sauvegarde sera refaite en le quittant.
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

void TestGui::duelModeIsChosenInSettings()
{
    SettingsPage page;
    QComboBox *duels = nullptr;
    for (QComboBox *combo : page.findChildren<QComboBox *>()) {
        if (combo->findData(static_cast<int>(DuelMode::Rush)) >= 0 && combo->count() == 3)
            duels = combo;
    }
    QVERIFY(duels);
    duels->setCurrentIndex(duels->findData(static_cast<int>(DuelMode::Rush)));
    emit duels->activated(duels->currentIndex());
    QVERIFY(MatchRules{}.duelMode == DuelMode::Rush); // Nouvelles parties : duels en rafale.
    MatchRules::setDefaultDuelMode(DuelMode::Timing);
    SettingsPage::loadPreferences(); // Relu des réglages au démarrage.
    QVERIFY(MatchRules{}.duelMode == DuelMode::Rush);

    duels->setCurrentIndex(duels->findData(static_cast<int>(DuelMode::Timing)));
    emit duels->activated(duels->currentIndex());
    QVERIFY(MatchRules{}.duelMode == DuelMode::Timing);
}

void TestGui::rulesOverrideTheDefaultDuelMode()
{
    RulesWidget widget;
    QComboBox *duels = nullptr;
    for (QComboBox *combo : widget.findChildren<QComboBox *>()) {
        if (combo->findData(static_cast<int>(DuelMode::Rush)) >= 0)
            duels = combo;
    }
    QVERIFY(duels);
    QVERIFY(widget.rules().duelMode == DuelMode::Timing);

    // Tant qu'il n'est pas choisi ici, le duel suit les paramètres.
    MatchRules::setDefaultDuelMode(DuelMode::None);
    QVERIFY(widget.rules().duelMode == DuelMode::None);
    QVERIFY(widget.rules().isDefault());

    // Choisi pour cette partie : il ne bouge plus, et le résumé le signale.
    duels->setCurrentIndex(duels->findData(static_cast<int>(DuelMode::Rush)));
    emit duels->activated(duels->currentIndex());
    MatchRules::setDefaultDuelMode(DuelMode::Timing);
    QVERIFY(widget.rules().duelMode == DuelMode::Rush);
    QVERIFY(widget.rules().summary().contains(toString(DuelMode::Rush)));

    buttonStartingWith(&widget, QStringLiteral("Réinitialiser"))->click();
    MatchRules::setDefaultDuelMode(DuelMode::None);
    QVERIFY(widget.rules().duelMode == DuelMode::None); // De nouveau celui des paramètres.
    MatchRules::setDefaultDuelMode(DuelMode::Timing);
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

void TestGui::soundsExistAndFollowTheBattle()
{
    const auto exists = [](const SoundCue &cue) { // Présent et au format du mélangeur des effets.
        return !Audio::loadPcm(QStringLiteral(":/audio/%1.wav").arg(Audio::fileName(cue))).isEmpty();
    };
    for (int s = 0; s < static_cast<int>(Sfx::Special); ++s)
        QVERIFY2(exists(static_cast<Sfx>(s)), qPrintable(Audio::fileName(static_cast<Sfx>(s))));
    for (const SpecialMove &move : SpecialMove::all())
        QVERIFY2(exists({Sfx::Special, 1.0, move.id}), qPrintable(move.id));
    QVERIFY(QFile::exists(QStringLiteral(":/audio/menu.wav")));
    QVERIFY(QFile::exists(QStringLiteral(":/audio/battle.wav")));
    QVERIFY(Audio::loadPcm(QStringLiteral(":/audio/menu.wav")).isEmpty()); // Stéréo : refusée.

    MatchState state;
    state.spinners.resize(2);
    BattleSounds sounds;
    const auto play = [&](const QList<ImpactEvent> &impacts = {}) {
        QList<Sfx> out;
        for (const SoundCue &cue : sounds.update(state, impacts, 0))
            out.append(cue.sfx);
        return out;
    };
    QVERIFY(play().isEmpty()); // Point de départ : aucun son.
    state.phase = MatchPhase::Countdown;
    state.countdown = 2.7;
    QVERIFY(play() == QList<Sfx>{Sfx::Countdown});
    state.countdown = 2.2;
    QVERIFY(play().isEmpty());
    state.countdown = 1.9;
    QVERIFY(play() == QList<Sfx>{Sfx::Countdown});
    state.phase = MatchPhase::Fighting;
    QVERIFY(play() == QList<Sfx>{Sfx::Go});

    ImpactEvent hit;
    hit.intensity = 0.5;
    ImpactEvent graze; // Frôlement du rebord : silencieux.
    graze.first = -1;
    graze.intensity = 0.01;
    state.spinners[1].dashTimer = 0.3;
    state.spinners[1].specialUsed = true;
    const QList<SoundCue> cues = sounds.update(state, {hit, graze}, 0);
    QCOMPARE(cues.size(), 3);
    QVERIFY(cues[0].sfx == Sfx::Hit && cues[1].sfx == Sfx::Dash && cues[2].sfx == Sfx::Special);
    QCOMPARE(cues[2].special, state.spinners[1].top.special());
    QVERIFY(play().isEmpty()); // Rien ne se répète tant que l'état ne change pas.

    state.duel.active = true;
    state.duel.first = 0;
    state.duel.second = 1;
    state.duel.sides[0].zone = 0.5;
    QVERIFY(play() == QList<Sfx>{Sfx::DuelStart});
    state.duel.sides[0].zone = 0.4; // Appui réussi : la zone rétrécit.
    QVERIFY(play() == QList<Sfx>{Sfx::DuelPress});
    ImpactEvent duelEnd;
    duelEnd.duelLoser = 1;
    state.duel.active = false;
    QVERIFY(play({duelEnd}) == QList<Sfx>{Sfx::DuelWin});

    state.spinners[1].status = Spinner::Status::RingOut;
    state.phase = MatchPhase::Finished;
    state.winner = 0;
    QVERIFY(play() == (QList<Sfx>{Sfx::Victory, Sfx::Ejection}));
}

void TestGui::soundSettingsAreSaved()
{
    SettingsPage page;
    auto *effects = page.findChildren<QSlider *>().value(Audio::Effects);
    QVERIFY(effects && !effects->isEnabled()); // Coupés par initTestCase.
    const QList<QCheckBox *> boxes = page.findChildren<QCheckBox *>();
    QCOMPARE(boxes.size(), Audio::kChannels);
    boxes[Audio::Effects]->setChecked(true);
    QVERIFY(effects->isEnabled());
    effects->setValue(35);
    QVERIFY(Audio::instance().isEnabled(Audio::Effects));
    QCOMPARE(Audio::instance().volume(Audio::Effects), 35);
    QCOMPARE(QSettings().value(QStringLiteral("audio/effects/volume")).toInt(), 35);
    QVERIFY(!Audio::instance().isEnabled(Audio::MenuMusic));
    boxes[Audio::Effects]->setChecked(false);
}

QTEST_MAIN(TestGui)
#include "tst_gui.moc"
