#pragma once

#include "game/BotController.h"
#include "net/MatchCodec.h"
#include "session/MatchSession.h"

#include <QPointer>

#include <memory>

namespace toupie::net {

class LobbyServer;

/// Combat en réseau côté hôte : l'hôte simule le combat (il fait autorité),
/// applique les visées et dashs reçus des clients et diffuse l'état à ses
/// participants. Les places sans joueur (playerId -1) sont tenues par des
/// bots. Sans serveur (`server` nul), c'est un combat local.
class HostSession : public MatchSession
{
    Q_OBJECT

public:
    HostSession(LobbyServer *server, const MatchSetup &setup, QObject *parent = nullptr);
    ~HostSession() override;

    void update(double dt) override;
    void submitAim(double angle, double power) override;
    void requestDash() override;
    void requestSpecial() override;
    void setSteering(Vec2 direction) override;

private:
    void onGameMessage(int playerId, const QJsonObject &message);
    void onPlayerLeft(int playerId);
    int slotOf(int playerId) const;
    void setAim(int slot, double angle, double power);
    void sendState();

    QPointer<LobbyServer> m_server;
    MatchSetup m_setup;
    std::unique_ptr<Battle> m_battle;
    QList<double> m_angles;
    QList<double> m_powers;
    QList<ImpactEvent> m_pendingImpacts;
    std::vector<BotController> m_bots;
    double m_aimTimer = 0.0;
    double m_sendTimer = 0.0;
};

} // namespace toupie::net
