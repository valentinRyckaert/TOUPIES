#pragma once

#include "net/MatchCodec.h"
#include "session/MatchSession.h"

#include <QPointer>

namespace toupie::net {

class LobbyClient;

/// Combat en réseau côté client : affiche l'état reçu de l'hôte (en
/// prolongeant les trajectoires entre deux réceptions) et lui transmet la
/// visée et les dashs du joueur local.
class ClientSession : public MatchSession
{
    Q_OBJECT

public:
    ClientSession(LobbyClient *client, const MatchSetup &setup, QObject *parent = nullptr);

    void update(double dt) override;
    void submitAim(double angle) override;
    void requestDash() override;

    /// Applique un message « state » de l'hôte (exposé pour les tests).
    void applyState(const QJsonObject &message);

private:
    QPointer<LobbyClient> m_client;
    double m_sinceState = 0.0;
};

} // namespace toupie::net
