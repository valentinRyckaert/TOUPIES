#pragma once

#include "core/Top.h"
#include "net/LobbyClient.h"
#include "net/LobbyServer.h"
#include "session/MatchSession.h"

#include <QList>
#include <QWidget>

#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace toupie {

class StadiumWidget;
class StatsWidget;
class TopRepository;

/// Lobby réseau, côté hôte ou côté client : joueurs connectés, choix de sa
/// toupie (parmi ses toupies locales), stadium et nombre de joueurs (hôte),
/// statut « prêt » et lancement de la partie.
class LobbyPage : public QWidget
{
    Q_OBJECT

public:
    explicit LobbyPage(TopRepository &repository, QWidget *parent = nullptr);
    ~LobbyPage() override;

    /// Crée un lobby (cette machine devient l'hôte).
    bool host(const net::LobbyServer::Settings &settings, QString *error);
    /// Rejoint un lobby ; le résultat arrive par joined() ou joinFailed().
    void join(const QString &host, quint16 port, const QString &playerName);
    /// Quitte (client) ou ferme (hôte) le lobby.
    void leave();
    /// Hôte : fin de partie, retour de tous les joueurs au lobby.
    void returnToLobby();

    bool isHost() const { return m_server != nullptr; }
    bool isActive() const { return m_server || m_client; }
    /// Session créée au lancement de la partie (la page en cède la propriété).
    std::unique_ptr<MatchSession> takeSession();

    /// Recharge la liste des toupies locales (après passage par l'atelier).
    void refreshTops();

signals:
    void joined();
    void joinFailed(const QString &reason);
    void connectionLost(const QString &reason);
    void leaveRequested();
    void matchStarted();
    void returnedToLobby();

private:
    const net::LobbyInfo *lobby() const;
    int localPlayerId() const;
    void rebuild();
    void sendSelectedTop();
    void startMatch();
    void onClientMatchStarted(const QJsonObject &start);
    void onLobbyChanged();

    TopRepository &m_repository;
    QList<Top> m_tops;
    std::unique_ptr<net::LobbyServer> m_server;
    std::unique_ptr<net::LobbyClient> m_client;
    std::unique_ptr<MatchSession> m_session;
    bool m_wasInGame = false;

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QComboBox *m_playersCombo = nullptr;
    QLabel *m_playersLabel = nullptr;
    QComboBox *m_stadiumCombo = nullptr;
    QLabel *m_stadiumLabel = nullptr;
    StadiumWidget *m_stadiumPreview = nullptr;
    QTableWidget *m_table = nullptr;
    QComboBox *m_topCombo = nullptr;
    StatsWidget *m_topPreview = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_readyButton = nullptr;
    QPushButton *m_startButton = nullptr;
};

} // namespace toupie
