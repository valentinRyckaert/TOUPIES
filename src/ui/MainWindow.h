#pragma once

#include "core/TopRepository.h"

#include <QMainWindow>
#include <QPointer>

#include <memory>

class QStackedWidget;

namespace toupie {

class BattlePage;
class BuilderPage;
class LanMenuPage;
class LobbyPage;
class MenuPage;
class SetupPage;
class TournamentPage;
class TournamentSetupPage;

namespace net {
class TournamentController;
class TournamentDirector;
}

/// Fenêtre principale : navigation entre les différents écrans.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupSolo();
    void setupLan();
    void setupTournaments();
    /// Affiche un tournoi (solo ou réseau) et suit les manches du joueur local.
    void attachTournament(net::TournamentController *tournament, bool host);
    void showPage(QWidget *page);
    void showMenu();
    void showBuilder();
    void showSetup();
    void showLanMenu();
    void leaveLobby(const QString &message = {});

    TopRepository m_repository;
    QStackedWidget *m_stack = nullptr;
    MenuPage *m_menu = nullptr;
    BuilderPage *m_builder = nullptr;
    SetupPage *m_setup = nullptr;
    BattlePage *m_battle = nullptr;
    LanMenuPage *m_lanMenu = nullptr;
    LobbyPage *m_lobby = nullptr;
    TournamentSetupPage *m_tournamentSetup = nullptr;
    TournamentPage *m_tournamentPage = nullptr;
    std::unique_ptr<net::TournamentDirector> m_soloTournament;
    QPointer<net::TournamentController> m_tournament;
};

} // namespace toupie
