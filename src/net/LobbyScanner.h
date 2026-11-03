#pragma once

#include "net/Protocol.h"

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QPair>

class QTcpSocket;

namespace toupie::net {

/// Découverte de secours par TCP : sonde les machines des sous-réseaux
/// locaux sur les ports du lobby et leur demande une description.
///
/// Les annonces UDP sont souvent bloquées (pare-feu qui n'autorise que le
/// port TCP du jeu, Wi-Fi qui filtre les diffusions) alors que la connexion
/// TCP fonctionne : ce balayage trouve alors quand même les lobbys.
class LobbyScanner : public QObject
{
    Q_OBJECT

public:
    explicit LobbyScanner(QObject *parent = nullptr);
    ~LobbyScanner() override;

    void setPorts(const QList<quint16> &ports) { m_ports = ports; }
    /// Sonde les adresses données.
    void scan(const QList<QHostAddress> &hosts);
    /// Sonde les sous-réseaux IPv4 locaux (hors interfaces virtuelles).
    void scanLocalNetworks();
    void cancel();
    bool isScanning() const { return !m_queue.isEmpty() || !m_active.isEmpty(); }

    /// Adresses à sonder : pour chaque interface physique, son /24 d'abord,
    /// puis le reste du sous-réseau dans la limite d'un /20.
    static QList<QHostAddress> localTargets();
    /// Idem, en séparant le /24 de chaque interface (`near`) du reste (`far`).
    static void localTargets(QList<QHostAddress> *near, QList<QHostAddress> *far);

signals:
    void lobbyFound(const toupie::net::LobbyAnnouncement &lobby);
    void finished();

private:
    void startNext();
    void finishProbe(QTcpSocket *socket);

    QList<quint16> m_ports;
    QList<QPair<QHostAddress, quint16>> m_queue;
    QList<QTcpSocket *> m_active;
};

} // namespace toupie::net
