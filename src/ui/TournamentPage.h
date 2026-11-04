#pragma once

#include <QPointer>
#include <QWidget>

class QGroupBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTreeWidget;

namespace toupie {

namespace net {
class TournamentController;
}

/// Tableau de bord d'un tournoi (solo ou réseau) : tours et résultats (panneau
/// latéral rétractable), classement de la ligue et match du joueur local
/// (l'ordre des toupies, en classique, se choisit dans une fenêtre).
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

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QTreeWidget *m_rounds = nullptr;
    QTableWidget *m_standings = nullptr;
    QGroupBox *m_matchBox = nullptr;
    QLabel *m_matchLabel = nullptr;
    QPushButton *m_ready = nullptr;
    QPushButton *m_leave = nullptr;
    QPushButton *m_returnToLobby = nullptr;
};

} // namespace toupie
