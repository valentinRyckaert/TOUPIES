#pragma once

#include "net/LobbyScanner.h"
#include "net/Protocol.h"

#include <QList>
#include <QObject>
#include <QTimer>

#include <optional>

class QUdpSocket;

namespace toupie::net {

/// Liste des lobbys visibles sur le réseau local.
///
/// Deux sources : les annonces UDP diffusées par les hôtes et, en secours,
/// un balayage TCP des sous-réseaux locaux (lancé à l'ouverture puis
/// régulièrement), qui fonctionne même quand l'UDP est filtré.
class LobbyBrowser : public QObject
{
    Q_OBJECT

public:
    explicit LobbyBrowser(quint16 discoveryPort = kDiscoveryPort, QObject *parent = nullptr);

    /// Démarre l'écoute UDP et, si `scanNetwork`, les balayages TCP.
    /// Retourne false si le port UDP n'a pas pu être ouvert (le balayage
    /// fonctionne quand même).
    bool start(bool scanNetwork = true);
    void stop();
    bool isRunning() const { return m_running; }

    /// Relance immédiatement un balayage des sous-réseaux.
    void rescan();
    bool isScanning() const { return m_scanner.isScanning(); }
    LobbyScanner &scanner() { return m_scanner; }

    /// Au moins une annonce UDP a été reçue depuis le démarrage.
    bool udpWorks() const { return m_udpReceived; }

    const QList<LobbyAnnouncement> &lobbies() const { return m_lobbies; }
    /// Lobby dont le nom correspond (sans tenir compte de la casse).
    std::optional<LobbyAnnouncement> findByName(const QString &name) const;

signals:
    void lobbiesChanged();
    void scanFinished();

private:
    void readDatagrams();
    void merge(LobbyAnnouncement lobby, bool fromUdp);
    void expire();

    quint16 m_port;
    QUdpSocket *m_socket = nullptr;
    LobbyScanner m_scanner;
    QTimer m_expireTimer;
    QTimer m_scanTimer;
    QList<LobbyAnnouncement> m_lobbies;
    QList<bool> m_fromUdp;
    bool m_running = false;
    bool m_udpReceived = false;
};

} // namespace toupie::net
