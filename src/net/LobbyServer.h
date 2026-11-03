#pragma once

#include "net/Protocol.h"

#include <QList>
#include <QObject>
#include <QTimer>

class QTcpServer;
class QUdpSocket;

namespace toupie {

class PartCatalog;

namespace net {

/// Serveur d'un lobby, exécuté chez l'hôte (qui est lui-même le joueur 0).
///
/// Accepte de 1 à 3 clients, tient l'état du lobby (joueurs, toupies,
/// stadium), le diffuse à chaque changement et annonce le lobby en UDP.
/// Pendant une partie, il relaie les messages de jeu vers HostSession.
class LobbyServer : public QObject
{
    Q_OBJECT

public:
    struct Settings
    {
        QString lobbyName;
        QString hostName;
        int maxPlayers = 2;
        QString stadiumId;
    };

    static constexpr int kHostId = 0;

    explicit LobbyServer(const PartCatalog &catalog, QObject *parent = nullptr);
    ~LobbyServer() override;

    /// Démarre l'écoute TCP (sur le premier port libre à partir de `port`, 0 =
    /// port quelconque) et, si `discoveryPort` est non nul, les annonces UDP.
    bool start(const Settings &settings, quint16 port = kDefaultTcpPort,
               quint16 discoveryPort = kDiscoveryPort);
    void close(const QString &reason = {});

    bool isListening() const;
    quint16 port() const;
    QString errorString() const { return m_error; }
    const LobbyInfo &lobby() const { return m_lobby; }

    void setHostTop(const Top &top);
    void setMaxPlayers(int maxPlayers);
    void setStadium(const QString &stadiumId);

    /// Lobby complet, toutes les toupies choisies, tous les clients prêts.
    bool canStart() const;
    /// Lance la partie : diffuse le message de départ et le renvoie.
    QJsonObject startMatch(quint32 seed);
    /// Fin de partie : retour de tous les joueurs au lobby.
    void returnToLobby();

    void sendTo(int playerId, const QJsonObject &message);
    void broadcast(const QJsonObject &message);

signals:
    void lobbyChanged();
    /// Message de jeu (visée, dash) reçu d'un client pendant une partie.
    void gameMessage(int playerId, const QJsonObject &message);
    void playerLeft(int playerId);

private:
    struct Client
    {
        int id = -1;          ///< -1 tant que le client ne s'est pas présenté.
        JsonChannel *channel = nullptr;
    };

    void onNewConnection();
    void onMessage(JsonChannel *channel, const QJsonObject &message);
    void onDisconnected(JsonChannel *channel);
    void handleHello(Client &client, const QJsonObject &message);
    void reject(JsonChannel *channel, const QString &reason);
    QJsonObject announcementJson() const;
    void announce();
    void lobbyUpdated();
    Client *clientFor(JsonChannel *channel);
    QString uniqueName(const QString &name) const;

    const PartCatalog &m_catalog;
    QString m_fingerprint;
    QTcpServer *m_server = nullptr;
    QUdpSocket *m_udp = nullptr;
    quint16 m_discoveryPort = 0;
    QTimer m_announceTimer;
    QList<Client> m_clients;
    LobbyInfo m_lobby;
    int m_nextId = 1;
    QString m_error;
};

} // namespace net
} // namespace toupie
