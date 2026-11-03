#include "net/LobbyClient.h"

#include "core/PartCatalog.h"

#include <QTcpSocket>

namespace toupie::net {

namespace {
constexpr int kConnectTimeoutMs = 5000;
}

LobbyClient::LobbyClient(const PartCatalog &catalog, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
{
    m_connectTimer.setSingleShot(true);
    m_connectTimer.setInterval(kConnectTimeoutMs);
    connect(&m_connectTimer, &QTimer::timeout, this,
            [this] { fail(tr("L'hôte ne répond pas.")); });
}

LobbyClient::~LobbyClient()
{
    if (m_channel) {
        disconnect(m_channel, nullptr, this, nullptr);
        m_channel->close();
    }
}

void LobbyClient::connectToHost(const QString &host, quint16 port, const QString &playerName)
{
    disconnectFromHost();
    m_playerName = playerName;
    m_hostLabel = QStringLiteral("%1:%2").arg(host).arg(port);
    m_closeReason.clear();

    auto *socket = new QTcpSocket;
    m_channel = new JsonChannel(socket, this);
    connect(m_channel, &JsonChannel::messageReceived, this, &LobbyClient::onMessage);
    connect(m_channel, &JsonChannel::disconnected, this, &LobbyClient::onDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
        if (!isJoined())
            fail(tr("Connexion impossible : %1").arg(socket->errorString()));
    });
    connect(socket, &QTcpSocket::connected, this, [this] {
        m_channel->send({
            {"type", Msg::Hello},
            {"app", kApplicationTag},
            {"version", kProtocolVersion},
            {"catalog", catalogFingerprint(m_catalog)},
            {"name", m_playerName},
        });
    });
    m_connectTimer.start();
    socket->connectToHost(host, port);
}

void LobbyClient::disconnectFromHost()
{
    m_connectTimer.stop();
    if (m_channel) {
        disconnect(m_channel, nullptr, this, nullptr);
        disconnect(m_channel->socket(), nullptr, this, nullptr);
        m_channel->close();
        m_channel->deleteLater();
        m_channel = nullptr;
    }
    m_playerId = -1;
    m_lobby = LobbyInfo{};
}

void LobbyClient::selectTop(const Top &top)
{
    send({{"type", Msg::SelectTop}, {"top", top.toJson()}});
}

void LobbyClient::setReady(bool ready)
{
    send({{"type", Msg::Ready}, {"ready", ready}});
}

void LobbyClient::send(const QJsonObject &message)
{
    if (m_channel)
        m_channel->send(message);
}

void LobbyClient::fail(const QString &reason)
{
    const bool wasJoined = isJoined();
    disconnectFromHost();
    if (wasJoined)
        emit connectionLost(reason);
    else
        emit joinFailed(reason);
}

void LobbyClient::onMessage(const QJsonObject &message)
{
    const QString type = message.value("type").toString();
    if (type == Msg::Welcome) {
        m_connectTimer.stop();
        m_playerId = message.value("playerId").toInt(-1);
        emit joined();
    } else if (type == Msg::Reject) {
        m_closeReason = message.value("reason").toString();
        fail(m_closeReason);
    } else if (type == Msg::Closed) {
        const QString reason = message.value("reason").toString();
        m_closeReason = reason.isEmpty() ? tr("L'hôte a fermé le lobby.") : reason;
    } else if (type == Msg::Lobby) {
        m_lobby = lobbyFromJson(message.value("lobby").toObject(), m_catalog);
        emit lobbyChanged();
    } else if (type == Msg::Start) {
        emit matchStarted(message);
    } else if (type == Msg::State) {
        emit gameMessage(message);
    }
}

void LobbyClient::onDisconnected()
{
    fail(m_closeReason.isEmpty() ? tr("Connexion avec l'hôte perdue.") : m_closeReason);
}

} // namespace toupie::net
