#pragma once

#include "net/LobbyScanner.h"
#include "net/Protocol.h"

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QThread>
#include <QTimer>

#include <optional>

class QUdpSocket;

namespace toupie::net {

/// Liste des lobbys visibles sur le réseau local.
///
/// Trois sources :
/// - les annonces UDP diffusées chaque seconde par les hôtes ;
/// - une question UDP diffusée toutes les 2 s, à laquelle les hôtes répondent
///   directement (une réponse passe souvent un pare-feu qui bloque les
///   annonces entrantes, celui de Windows notamment) ;
/// - en secours, un balayage TCP des sous-réseaux locaux, dans un fil séparé
///   (l'interface ne gèle jamais) : le /24 local toutes les 4 s, le reste du
///   sous-réseau toutes les 30 s.
class LobbyBrowser : public QObject
{
    Q_OBJECT

public:
    explicit LobbyBrowser(quint16 discoveryPort = kDiscoveryPort, QObject *parent = nullptr);
    ~LobbyBrowser() override;

    /// Démarre l'écoute UDP et, si `scanNetwork`, les balayages TCP.
    /// Retourne false si le port UDP n'a pas pu être ouvert (le balayage
    /// fonctionne quand même).
    bool start(bool scanNetwork = true);
    void stop();
    bool isRunning() const { return m_running; }

    /// Relance immédiatement une question UDP et un balayage complet.
    void rescan();
    bool isScanning() const { return m_scanning; }

    /// Au moins une annonce UDP a été reçue depuis le démarrage.
    bool udpWorks() const { return m_udpReceived; }

    const QList<LobbyAnnouncement> &lobbies() const { return m_lobbies; }
    /// Lobby dont le nom correspond (sans tenir compte de la casse).
    std::optional<LobbyAnnouncement> findByName(const QString &name) const;

signals:
    void lobbiesChanged();
    void scanFinished();

private:
    void readDatagrams(QUdpSocket *socket);
    void sendQuery();
    void scan(bool nearOnly);
    void merge(LobbyAnnouncement lobby, bool fromUdp);
    void expire();

    quint16 m_port;
    QUdpSocket *m_socket = nullptr;
    QUdpSocket *m_querySocket = nullptr;
    QList<QHostAddress> m_queryTargets;
    QThread m_scanThread;
    LobbyScanner *m_scanner = nullptr; ///< Vit dans m_scanThread.
    bool m_scanning = false;
    QTimer m_expireTimer;
    QTimer m_scanTimer;
    QTimer m_nearTimer;
    QTimer m_queryTimer;
    QList<LobbyAnnouncement> m_lobbies;
    QList<bool> m_fromUdp;
    bool m_running = false;
    bool m_udpReceived = false;
};

} // namespace toupie::net
