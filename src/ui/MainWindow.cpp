#include "ui/MainWindow.h"

#include "core/PartCatalog.h"
#include "game/Tournament.h"
#include "net/TournamentSession.h"
#include "net/Updater.h"
#include "session/LocalSession.h"
#include "ui/Audio.h"
#include "ui/BattlePage.h"
#include "ui/BuilderPage.h"
#include "ui/LanMenuPage.h"
#include "ui/HelpPage.h"
#include "ui/LobbyPage.h"
#include "ui/MenuPage.h"
#include "ui/MovesBoard.h"
#include "ui/SettingsPage.h"
#include "ui/SetupPage.h"
#include "ui/TournamentPage.h"
#include "ui/TournamentSetupPage.h"
#include "ui/TutorialSession.h"

#include "ui/Theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QMessageBox>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>

namespace toupie {

namespace {
/// Fond holographique commun à tous les écrans (mis en cache à chaque taille).
class Backdrop : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        const qreal dpr = devicePixelRatioF();
        if (m_cache.size() != size() * dpr) {
            m_cache = QPixmap(size() * dpr);
            m_cache.setDevicePixelRatio(dpr);
            QPainter cache(&m_cache);
            Theme::paintBackground(cache, rect());
        }
        QPainter(this).drawPixmap(0, 0, m_cache);
    }

private:
    QPixmap m_cache;
};
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_repository(PartCatalog::instance())
{
    setWindowTitle(QStringLiteral("Top Arena")); // Nom du jeu : jamais traduit.
    resize(1180, 860);

    const bool loaded = m_repository.load();

    auto *backdrop = new Backdrop(this);
    auto *backdropLayout = new QVBoxLayout(backdrop);
    backdropLayout->setContentsMargins(16, 12, 16, 12);
    m_stack = new QStackedWidget(backdrop);
    backdropLayout->addWidget(m_stack);
    m_menu = new MenuPage(m_repository, m_stack);
    m_builder = new BuilderPage(PartCatalog::instance(), m_repository, m_stack);
    m_setup = new SetupPage(m_repository, m_stack);
    m_battle = new BattlePage(m_stack);
    m_lanMenu = new LanMenuPage(m_stack);
    m_lobby = new LobbyPage(m_repository, m_stack);
    m_tournamentSetup = new TournamentSetupPage(m_repository, m_stack);
    m_tournamentPage = new TournamentPage(m_stack);
    m_settingsPage = new SettingsPage(m_stack);
    m_helpPage = new HelpPage(m_stack);
    m_movesPage = new MovesPage(m_stack);
    for (QWidget *page : {static_cast<QWidget *>(m_menu), static_cast<QWidget *>(m_builder),
                          static_cast<QWidget *>(m_setup), static_cast<QWidget *>(m_battle),
                          static_cast<QWidget *>(m_lanMenu), static_cast<QWidget *>(m_lobby),
                          static_cast<QWidget *>(m_tournamentSetup),
                          static_cast<QWidget *>(m_tournamentPage),
                          static_cast<QWidget *>(m_settingsPage), static_cast<QWidget *>(m_helpPage),
                          static_cast<QWidget *>(m_movesPage)})
        m_stack->addWidget(page);
    setCentralWidget(backdrop);
    // Le jeu démarre en plein écran (main.cpp) ; F11 bascule, Échap restant le retour.
    auto *fullScreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullScreen->setContext(Qt::ApplicationShortcut);
    connect(fullScreen, &QShortcut::activated, this,
            [this] { setWindowState(windowState() ^ Qt::WindowFullScreen); });

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
    connect(m_menu, &MenuPage::updateRequested, this, &MainWindow::checkForUpdates);
    connect(m_menu, &MenuPage::tutorialRequested, this, [this] {
        showPage(m_battle);
        m_battle->start(std::make_unique<TutorialSession>(), BattlePage::Mode::Tutorial);
    });
    auto showSettings = [this] {
        m_settingsPage->refresh();
        showPage(m_settingsPage);
    };
    auto showHelp = [this] {
        m_helpPage->refresh();
        showPage(m_helpPage);
    };
    connect(m_menu, &MenuPage::settingsRequested, this, showSettings);
    connect(m_menu, &MenuPage::helpRequested, this, showHelp);
    connect(m_helpPage, &HelpPage::settingsRequested, this, showSettings);
    connect(m_settingsPage, &SettingsPage::backRequested, this, &MainWindow::showMenu);
    connect(m_settingsPage, &SettingsPage::restartRequested, this, [this] {
        QString error;
        if (net::Updater::restart(&error))
            close();
        else
            QMessageBox::information(this, tr("Langue"),
                                     tr("Relancez le jeu pour appliquer la langue.\n%1").arg(error));
    });
    connect(m_helpPage, &HelpPage::backRequested, this, &MainWindow::showMenu);
    connect(m_builder, &BuilderPage::backRequested, this, &MainWindow::showMenu);
    connect(m_builder, &BuilderPage::movesRequested, this, [this] { showPage(m_movesPage); });
    // Retour à l'atelier tel qu'on l'a laissé (sans le recharger).
    connect(m_movesPage, &MovesPage::backRequested, this, [this] { showPage(m_builder); });
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
    // Tournoi solo en cours : sauvegardé d'office, on le reprend au prochain lancement.
    if (m_soloTournament && !m_soloTournament->tournament().isFinished())
        SavedTournament::save(m_soloTournament->tournament());
    m_soloTournament.reset();
    m_lobby->leave();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupSolo()
{
    connect(m_setup, &SetupPage::backRequested, this, &MainWindow::showMenu);
    connect(m_setup, &SetupPage::fightRequested, this,
            [this](const Top &player, const QList<Top> &opponents, Difficulty difficulty,
                   const QString &stadiumId, const MatchRules &rules) {
                const Stadium stadium = Stadium::find(stadiumId).value_or(Stadium::classic());
                QList<Top> tops{player};
                QStringList names{tr("Vous")};
                for (int i = 0; i < opponents.size(); ++i) {
                    tops.append(opponents.at(i));
                    // Seul, le bot porte sa difficulté ; en mêlée, un numéro.
                    names.append(tr("Bot %1").arg(opponents.size() == 1 ? toString(difficulty)
                                                                        : QString::number(i + 1)));
                }
                auto session = std::make_unique<LocalSession>(tops, names, stadium, difficulty,
                                                              rules);
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
        } else if (m_battle->mode() == BattlePage::Mode::Solo
                   || m_battle->mode() == BattlePage::Mode::Tutorial) {
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
        case BattlePage::Mode::Tutorial:
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
    connect(m_tournamentSetup, &TournamentSetupPage::resumeRequested, this, [this] {
        auto saved = SavedTournament::load(PartCatalog::instance());
        if (!saved) {
            QMessageBox::warning(this, tr("Tournoi illisible"),
                                 tr("Le tournoi sauvegardé est illisible ; il a été supprimé."));
            SavedTournament::remove();
            m_tournamentSetup->refresh();
            return;
        }
        // Le tournoi repris est en cours : la sauvegarde sera refaite en le quittant.
        SavedTournament::remove();
        m_soloTournament = std::make_unique<net::TournamentDirector>(
            nullptr, std::move(*saved), QRandomGenerator::global()->generate());
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
            if (!m_soloTournament->tournament().isFinished()) {
                QMessageBox box(QMessageBox::Question, tr("Quitter le tournoi"),
                                tr("Sauvegarder le tournoi pour le reprendre plus tard ? Un match "
                                   "en cours sera rejoué depuis le début."),
                                QMessageBox::NoButton, this);
                QPushButton *save = box.addButton(tr("Sauvegarder et quitter"), QMessageBox::AcceptRole);
                QPushButton *drop = box.addButton(tr("Quitter sans sauvegarder"), QMessageBox::DestructiveRole);
                box.addButton(tr("Annuler"), QMessageBox::RejectRole);
                save->setProperty("variant", QStringLiteral("primary"));
                drop->setProperty("variant", QStringLiteral("danger"));
                box.setDefaultButton(save);
                box.exec();
                if (box.clickedButton() == save) {
                    if (!SavedTournament::save(m_soloTournament->tournament())) {
                        QMessageBox::warning(this, tr("Sauvegarde impossible"),
                                             tr("Impossible d'écrire le fichier :\n%1")
                                                 .arg(SavedTournament::filePath()));
                        return;
                    }
                } else if (box.clickedButton() != drop) {
                    return;
                }
            }
            m_battle->stop();
            m_soloTournament.reset();
            showMenu();
        } else {
            if (QMessageBox::question(this, tr("Quitter le tournoi"), tr("Quitter le tournoi en réseau ?"))
                != QMessageBox::Yes)
                return;
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
    if (page != m_battle) {
        m_battle->stop();
        Audio::instance().setMusic(Audio::Music::Menu); // Combat : l'arène choisit la musique.
    }
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

void MainWindow::checkForUpdates()
{
    auto *updater = new net::Updater(this);
    auto *progress = new QProgressDialog(tr("Recherche d'une mise à jour…"), tr("Annuler"), 0, 0,
                                         this);
    progress->setWindowTitle(tr("Mises à jour"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    // Annuler détruit l'Updater, ce qui interrompt ses requêtes.
    connect(progress, &QProgressDialog::canceled, updater, &QObject::deleteLater);
    connect(updater, &QObject::destroyed, progress, &QObject::deleteLater);
    auto done = [updater, progress] {
        progress->hide();
        updater->deleteLater();
    };
    const QString current = QCoreApplication::applicationVersion();

    connect(updater, &net::Updater::failed, this, [this, done](const QString &reason) {
        done();
        QMessageBox::warning(this, tr("Mises à jour"),
                             tr("La mise à jour a échoué :\n%1").arg(reason));
    });
    connect(updater, &net::Updater::upToDate, this, [this, done, current] {
        done();
        QMessageBox::information(this, tr("Mises à jour"),
                                 tr("Vous avez la dernière version (%1).").arg(current));
    });
    connect(updater, &net::Updater::updateAvailable, this,
            [this, updater, progress, done, current](const net::UpdateInfo &update) {
                progress->hide();
                if (!net::Updater::canSelfUpdate()) {
                    done();
                    // Compilé depuis les sources, paquet de distribution ou dossier protégé.
                    if (QMessageBox::question(
                            this, tr("Mises à jour"),
                            tr("La version %1 est disponible (vous avez la %2), mais cette "
                               "installation ne peut pas se mettre à jour seule.\nOuvrir la page "
                               "de téléchargement ?")
                                .arg(update.version, current))
                        == QMessageBox::Yes)
                        QDesktopServices::openUrl(update.page);
                    return;
                }
                if (QMessageBox::question(this, tr("Mises à jour"),
                                          tr("La version %1 est disponible (vous avez la %2).\n"
                                             "La télécharger et l'installer ? Le jeu redémarrera "
                                             "sur la nouvelle version.")
                                              .arg(update.version, current))
                    != QMessageBox::Yes) {
                    done();
                    return;
                }
                progress->setLabelText(tr("Téléchargement de la version %1…").arg(update.version));
                progress->show();
                updater->download(update);
            });
    connect(updater, &net::Updater::progress, progress, [progress](qint64 received, qint64 total) {
        progress->setMaximum(static_cast<int>(std::max<qint64>(0, total / 1024)));
        progress->setValue(static_cast<int>(received / 1024));
    });
    connect(updater, &net::Updater::downloaded, this, [this, done](const QString &package) {
        done();
        QString error;
        if (net::Updater::installAndRestart(package, &error))
            close(); // La nouvelle version prend le relais.
        else
            QMessageBox::warning(this, tr("Mises à jour"),
                                 tr("L'installation a échoué :\n%1").arg(error));
    });
    updater->check();
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
