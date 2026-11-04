#pragma once

#include "net/Protocol.h"

#include <QObject>
#include <QTimer>

namespace toupie {

class PartCatalog;

namespace net {

/// Connexion d'un joueur au lobby d'un hôte.
class LobbyClient : public QObject
{
    Q_OBJECT

public:
    explicit LobbyClient(const PartCatalog &catalog, QObject *parent = nullptr);
    ~LobbyClient() override;

    void connectToHost(const QString &host, quint16 port, const QString &playerName);
    void disconnectFromHost();

    bool isJoined() const { return m_playerId >= 0; }
    int playerId() const { return m_playerId; }
    const LobbyInfo &lobby() const { return m_lobby; }
    QString hostLabel() const { return m_hostLabel; }

    void selectTops(const QList<Top> &tops);
    void setReady(bool ready);
    /// Message de jeu (visée, dash, duel…).
    void send(const QJsonObject &message);

signals:
    void joined();
    void lobbyChanged();
    void matchStarted(const QJsonObject &start);
    void gameMessage(const QJsonObject &message);
    /// Échec de connexion ou refus de l'hôte (avant d'avoir rejoint).
    void joinFailed(const QString &reason);
    /// Perte de connexion après avoir rejoint.
    void connectionLost(const QString &reason);

private:
    void onMessage(const QJsonObject &message);
    void onDisconnected();
    void fail(const QString &reason);

    const PartCatalog &m_catalog;
    JsonChannel *m_channel = nullptr;
    QTimer m_connectTimer;
    QString m_playerName;
    QString m_hostLabel;
    QString m_closeReason;
    LobbyInfo m_lobby;
    int m_playerId = -1;
};

} // namespace net
} // namespace toupie
