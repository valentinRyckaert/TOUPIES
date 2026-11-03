#pragma once

#include "net/Protocol.h"

#include <QList>
#include <QObject>
#include <QTimer>

#include <optional>

class QUdpSocket;

namespace toupie::net {

/// Écoute les annonces UDP des hôtes et tient la liste des lobbys visibles.
class LobbyBrowser : public QObject
{
    Q_OBJECT

public:
    explicit LobbyBrowser(quint16 discoveryPort = kDiscoveryPort, QObject *parent = nullptr);

    bool start();
    void stop();
    bool isRunning() const { return m_socket != nullptr; }

    const QList<LobbyAnnouncement> &lobbies() const { return m_lobbies; }
    /// Lobby dont le nom correspond (sans tenir compte de la casse).
    std::optional<LobbyAnnouncement> findByName(const QString &name) const;

signals:
    void lobbiesChanged();

private:
    void readDatagrams();
    void expire();

    quint16 m_port;
    QUdpSocket *m_socket = nullptr;
    QTimer m_expireTimer;
    QList<LobbyAnnouncement> m_lobbies;
};

} // namespace toupie::net
