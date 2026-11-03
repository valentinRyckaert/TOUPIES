#pragma once

#include "net/MatchCodec.h"
#include "session/MatchSession.h"

#include <QPointer>

#include <memory>

namespace toupie::net {

class LobbyServer;

/// Combat en réseau côté hôte : l'hôte simule le combat (il fait autorité),
/// applique les visées et dashs reçus des clients et leur diffuse l'état.
class HostSession : public MatchSession
{
    Q_OBJECT

public:
    HostSession(LobbyServer *server, const MatchSetup &setup, QObject *parent = nullptr);
    ~HostSession() override;

    void update(double dt) override;
    void submitAim(double angle) override;
    void requestDash() override;

private:
    void onGameMessage(int playerId, const QJsonObject &message);
    void onPlayerLeft(int playerId);
    int slotOf(int playerId) const;
    void setAim(int slot, double angle);
    void sendState();

    QPointer<LobbyServer> m_server;
    MatchSetup m_setup;
    std::unique_ptr<Battle> m_battle;
    QList<double> m_angles;
    QList<ImpactEvent> m_pendingImpacts;
    double m_aimTimer = 0.0;
    double m_sendTimer = 0.0;
};

} // namespace toupie::net
