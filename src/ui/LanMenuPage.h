#pragma once

#include "net/LobbyBrowser.h"
#include "net/LobbyServer.h"

#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace toupie {

class StadiumWidget;

/// Écran du multijoueur en réseau local : créer un lobby ou en rejoindre
/// un (dans la liste des lobbys découverts, par son nom ou par adresse IP).
class LanMenuPage : public QWidget
{
    Q_OBJECT

public:
    explicit LanMenuPage(QWidget *parent = nullptr);

    QString playerName() const;
    /// Démarre / arrête l'écoute des lobbys (uniquement quand l'écran est affiché).
    void activate();
    void deactivate();
    void setBusy(bool busy, const QString &message = {});
    /// Mode tournoi : le lobby créé est un lobby de tournoi avec ces réglages.
    void setTournament(const std::optional<TournamentSettings> &settings);

signals:
    void hostRequested(const toupie::net::LobbyServer::Settings &settings);
    void joinRequested(const QString &host, quint16 port);
    void backRequested();

private:
    void refreshLobbies();
    void updateDiscoveryStatus();
    void joinSelected();
    void joinTyped();
    void requestHost();

    net::LobbyBrowser m_browser;
    std::optional<TournamentSettings> m_tournament;
    QLabel *m_title = nullptr;
    QGroupBox *m_createBox = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_lobbyNameEdit = nullptr;
    QComboBox *m_playersCombo = nullptr;
    QComboBox *m_stadiumCombo = nullptr;
    StadiumWidget *m_stadiumPreview = nullptr;
    QTreeWidget *m_lobbyList = nullptr;
    QLineEdit *m_joinEdit = nullptr;
    QPushButton *m_createButton = nullptr;
    QPushButton *m_joinSelectedButton = nullptr;
    QPushButton *m_joinButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_discoveryStatus = nullptr;
    bool m_udpAvailable = true;
    QLabel *m_status = nullptr;
};

} // namespace toupie
