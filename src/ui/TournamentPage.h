#pragma once

#include <QPointer>
#include <QWidget>

class QGroupBox;
class QPushButton;
class QScrollArea;

namespace toupie {

class TopBar;

namespace net {
class TournamentController;
}

/// Tableau de bord d'un tournoi (solo ou réseau), d'après la maquette : tableau
/// à élimination directe (ou classement de la ligue), match du joueur local
/// et liste des tours et résultats ; l'ordre des toupies, en classique, se
/// choisit dans une fenêtre.
class TournamentPage : public QWidget
{
    Q_OBJECT

public:
    explicit TournamentPage(QWidget *parent = nullptr);

    /// Tournoi affiché (non possédé) ; `host` : peut ramener le lobby à la fin.
    void setController(net::TournamentController *controller, bool host = false);

signals:
    void leaveRequested();
    void returnToLobbyRequested();

private:
    void rebuild();
    QString participantLabel(int participant) const;

    QPointer<net::TournamentController> m_controller;
    bool m_host = false;

    TopBar *m_bar = nullptr;
    QWidget *m_chips[3] = {};
    QGroupBox *m_boardBox = nullptr;
    QScrollArea *m_bracketScroll = nullptr;
    QWidget *m_bracket = nullptr;
    QScrollArea *m_standingsScroll = nullptr;
    QWidget *m_standings = nullptr;
    QGroupBox *m_matchBox = nullptr;
    QWidget *m_matchCard = nullptr;
    QPushButton *m_ready = nullptr;
    QGroupBox *m_resultsBox = nullptr;
    QScrollArea *m_resultsScroll = nullptr;
    QWidget *m_results = nullptr;
    QPushButton *m_returnToLobby = nullptr;
};

} // namespace toupie
