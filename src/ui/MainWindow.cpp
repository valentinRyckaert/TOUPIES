#include "ui/MainWindow.h"

#include "core/PartCatalog.h"
#include "net/TournamentSession.h"
#include "session/LocalSession.h"
#include "ui/BattlePage.h"
#include "ui/BuilderPage.h"
#include "ui/LanMenuPage.h"
#include "ui/LobbyPage.h"
#include "ui/MenuPage.h"
#include "ui/SetupPage.h"
#include "ui/TournamentPage.h"
#include "ui/TournamentSetupPage.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QTimer>

namespace toupie {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_repository(PartCatalog::instance())
{
    setWindowTitle(tr("Toupie Arena"));
    resize(940, 880);

    const bool loaded = m_repository.load();

    m_stack = new QStackedWidget(this);
    m_menu = new MenuPage(m_stack);
    m_builder = new BuilderPage(PartCatalog::instance(), m_repository, m_stack);
    m_setup = new SetupPage(m_repository, m_stack);
    m_battle = new BattlePage(m_stack);
    m_lanMenu = new LanMenuPage(m_stack);
    m_lobby = new LobbyPage(m_repository, m_stack);
    m_tournamentSetup = new TournamentSetupPage(m_repository, m_stack);
    m_tournamentPage = new TournamentPage(m_stack);
    for (QWidget *page : {static_cast<QWidget *>(m_menu), static_cast<QWidget *>(m_builder),
                          static_cast<QWidget *>(m_setup), static_cast<QWidget *>(m_battle),
                          static_cast<QWidget *>(m_lanMenu), static_cast<QWidget *>(m_lobby),
                          static_cast<QWidget *>(m_tournamentSetup),
                          static_cast<QWidget *>(m_tournamentPage)})
        m_stack->addWidget(page);
    setCentralWidget(m_stack);

    connect(m_menu, &MenuPage::playRequested, this, &MainWindow::showSetup);
    connect(m_menu, &MenuPage::lanRequested, this, [this] {
        m_lanMenu->setTournament(std::nullopt);
        showLanMenu();
    });
    connect(m_menu, &MenuPage::tournamentRequested, this, [this] {
        m_tournamentSetup->refresh();
        showPage(m_tournamentSetup);
    });
    connect(m_menu, &MenuPage::builderRequested, this, &MainWindow::showBuilder);
    connect(m_menu, &MenuPage::quitRequested, this, &QWidget::close);
    connect(m_builder, &BuilderPage::backRequested, this, &MainWindow::showMenu);
    connect(m_builder, &BuilderPage::topsChanged, m_setup, &SetupPage::refresh);
    connect(m_builder, &BuilderPage::topsChanged, m_lobby, &LobbyPage::refreshTops);

    setupSolo();
    setupLan();
    setupTournaments();
    showMenu();

    if (!loaded) {
        QTimer::singleShot(0, this, [this] {
            QMessageBox::warning(this, tr("Toupies sauvegardées"),
                                 tr("Le fichier des toupies est illisible et a été ignoré :\n%1")
                                     .arg(m_repository.filePath()));
        });
    }
}

MainWindow::~MainWindow()
{
    // La session réseau doit disparaître avant le lobby qu'elle utilise.
    m_battle->stop();
    m_soloTournament.reset();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_battle->stop();
    m_soloTournament.reset();
    m_lobby->leave();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupSolo()
{
    connect(m_setup, &SetupPage::backRequested, this, &MainWindow::showMenu);
    connect(m_setup, &SetupPage::fightRequested, this,
            [this](const Top &player, const Top &opponent, Difficulty difficulty,
                   const QString &stadiumId, const MatchRules &rules) {
                const Stadium stadium = Stadium::find(stadiumId).value_or(Stadium::classic());
                auto session = std::make_unique<LocalSession>(
                    QList<Top>{player, opponent},
                    QStringList{tr("Vous"), tr("Bot %1").arg(toString(difficulty))}, stadium,
                    difficulty, rules);
                showPage(m_battle);
                m_battle->start(std::move(session), BattlePage::Mode::Solo);
            });
    connect(m_battle, &BattlePage::setupRequested, this, &MainWindow::showSetup);
    connect(m_battle, &BattlePage::leaveRequested, this, [this] {
        if (m_battle->mode() == BattlePage::Mode::Tournament) {
            if (m_tournament
                && QMessageBox::question(this, tr("Abandonner le match"),
                                         tr("Abandonner ce match ? Votre adversaire le remportera.")
                                         ) == QMessageBox::Yes)
                m_tournament->forfeitLocal();
        } else if (m_battle->mode() == BattlePage::Mode::Solo) {
            showMenu();
        } else {
            leaveLobby();
        }
    });
    connect(m_battle, &BattlePage::rematchRequested, this, [this] {
        switch (m_battle->mode()) {
        case BattlePage::Mode::Solo:
            if (auto *local = qobject_cast<LocalSession *>(m_battle->session()))
                local->restart();
            break;
        case BattlePage::Mode::LanHost:
            m_battle->stop();
            m_lobby->returnToLobby();
            showPage(m_lobby);
            break;
        case BattlePage::Mode::LanClient:
        case BattlePage::Mode::Tournament:
            break;
        }
    });
}

void MainWindow::setupTournaments()
{
    connect(m_tournamentSetup, &TournamentSetupPage::backRequested, this, &MainWindow::showMenu);
    connect(m_tournamentSetup, &TournamentSetupPage::soloRequested, this,
            [this](const TournamentSettings &settings, const QList<Top> &tops) {
                auto *rng = QRandomGenerator::global();
                const QList<Participant> humans{{tr("Vous"), 0, Difficulty::Normal, tops}};
                Tournament tournament(settings,
                                      addBots(humans, settings, m_repository.allTops(),
                                              rng->generate()),
                                      rng->generate());
                m_soloTournament = std::make_unique<net::TournamentDirector>(
                    nullptr, std::move(tournament), rng->generate());
                attachTournament(m_soloTournament.get(), false);
            });
    connect(m_tournamentSetup, &TournamentSetupPage::lanRequested, this, [this] {
        m_lanMenu->setTournament(m_tournamentSetup->settings());
        showLanMenu();
    });
    connect(m_lobby, &LobbyPage::tournamentStarted, this,
            [this] { attachTournament(m_lobby->tournament(), m_lobby->isHost()); });
    connect(m_tournamentPage, &TournamentPage::leaveRequested, this, [this] {
        if (m_tournament && m_tournament == m_soloTournament.get()) {
            if (QMessageBox::question(this, tr("Quitter le tournoi"),
                                      tr("Quitter le tournoi ? Il sera perdu."))
                != QMessageBox::Yes)
                return;
            m_battle->stop();
            m_soloTournament.reset();
            showMenu();
        } else {
            leaveLobby();
        }
    });
    connect(m_tournamentPage, &TournamentPage::returnToLobbyRequested, this, [this] {
        m_battle->stop();
        m_lobby->returnToLobby();
        showPage(m_lobby);
    });
}

void MainWindow::attachTournament(net::TournamentController *tournament, bool host)
{
    m_tournament = tournament;
    m_tournamentPage->setController(tournament, host);
    if (!tournament)
        return;
    connect(tournament, &net::TournamentController::sessionReady, this, [this, tournament] {
        showPage(m_battle);
        m_battle->start(tournament->takeSession(), BattlePage::Mode::Tournament);
    });
    connect(tournament, &net::TournamentController::localMatchOver, this,
            [this] { showPage(m_tournamentPage); });
    showPage(m_tournamentPage);
}

void MainWindow::setupLan()
{
    connect(m_lanMenu, &LanMenuPage::backRequested, this, &MainWindow::showMenu);
    connect(m_lanMenu, &LanMenuPage::hostRequested, this,
            [this](const net::LobbyServer::Settings &settings) {
                QString error;
                if (m_lobby->host(settings, &error)) {
                    showPage(m_lobby);
                } else {
                    QMessageBox::warning(this, tr("Création du lobby impossible"),
                                         tr("Le lobby n'a pas pu être créé :\n%1").arg(error));
                }
            });
    connect(m_lanMenu, &LanMenuPage::joinRequested, this,
            [this](const QString &host, quint16 port) {
                m_lanMenu->setBusy(true, tr("Connexion à %1:%2…").arg(host).arg(port));
                m_lobby->join(host, port, m_lanMenu->playerName());
            });

    connect(m_lobby, &LobbyPage::joined, this, [this] {
        m_lanMenu->setBusy(false);
        showPage(m_lobby);
    });
    connect(m_lobby, &LobbyPage::joinFailed, this, [this](const QString &reason) {
        m_lanMenu->setBusy(false);
        QMessageBox::warning(this, tr("Impossible de rejoindre"), reason);
    });
    connect(m_lobby, &LobbyPage::connectionLost, this, [this](const QString &reason) {
        m_battle->stop();
        showLanMenu();
        QMessageBox::information(this, tr("Lobby fermé"), reason);
    });
    connect(m_lobby, &LobbyPage::leaveRequested, this, [this] { leaveLobby(); });
    connect(m_lobby, &LobbyPage::matchStarted, this, [this] {
        showPage(m_battle);
        m_battle->start(m_lobby->takeSession(), m_lobby->isHost() ? BattlePage::Mode::LanHost
                                                                  : BattlePage::Mode::LanClient);
    });
    connect(m_lobby, &LobbyPage::returnedToLobby, this, [this] {
        m_battle->stop();
        showPage(m_lobby);
    });
}

void MainWindow::showPage(QWidget *page)
{
    if (page != m_lanMenu)
        m_lanMenu->deactivate();
    if (page != m_battle)
        m_battle->stop();
    m_stack->setCurrentWidget(page);
}

void MainWindow::showMenu()
{
    showPage(m_menu);
}

void MainWindow::showBuilder()
{
    m_builder->refresh();
    showPage(m_builder);
}

void MainWindow::showSetup()
{
    m_setup->refresh();
    showPage(m_setup);
}

void MainWindow::showLanMenu()
{
    m_lanMenu->setBusy(false);
    m_lanMenu->activate();
    showPage(m_lanMenu);
}

void MainWindow::leaveLobby(const QString &message)
{
    m_battle->stop();
    m_lobby->leave();
    showLanMenu();
    if (!message.isEmpty())
        QMessageBox::information(this, tr("Lobby"), message);
}

} // namespace toupie
