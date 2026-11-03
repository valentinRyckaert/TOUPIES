#include "net/LobbyBrowser.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QUdpSocket>

#include <algorithm>

namespace toupie::net {

namespace {
constexpr qint64 kLobbyLifetimeMs = 3500;
}

LobbyBrowser::LobbyBrowser(quint16 discoveryPort, QObject *parent)
    : QObject(parent)
    , m_port(discoveryPort)
{
    m_expireTimer.setInterval(1000);
    connect(&m_expireTimer, &QTimer::timeout, this, &LobbyBrowser::expire);
}

bool LobbyBrowser::start()
{
    if (m_socket)
        return true;
    m_socket = new QUdpSocket(this);
    // Adresse partagée : plusieurs instances du jeu peuvent écouter sur la même machine.
    if (!m_socket->bind(QHostAddress::AnyIPv4, m_port,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        delete m_socket;
        m_socket = nullptr;
        return false;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &LobbyBrowser::readDatagrams);
    m_expireTimer.start();
    return true;
}

void LobbyBrowser::stop()
{
    m_expireTimer.stop();
    delete m_socket;
    m_socket = nullptr;
    if (!m_lobbies.isEmpty()) {
        m_lobbies.clear();
        emit lobbiesChanged();
    }
}

std::optional<LobbyAnnouncement> LobbyBrowser::findByName(const QString &name) const
{
    for (const LobbyAnnouncement &lobby : m_lobbies) {
        if (lobby.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            return lobby;
    }
    return std::nullopt;
}

void LobbyBrowser::readDatagrams()
{
    bool changed = false;
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram(4096);
        const QJsonDocument doc = QJsonDocument::fromJson(datagram.data());
        const QJsonObject o = doc.object();
        if (o.value("app").toString() != kApplicationTag
            || o.value("v").toInt() != kProtocolVersion)
            continue;

        LobbyAnnouncement lobby;
        lobby.lobbyId = o.value("id").toString();
        lobby.name = o.value("name").toString();
        lobby.hostName = o.value("host").toString();
        lobby.stadiumId = o.value("stadium").toString();
        lobby.port = static_cast<quint16>(o.value("port").toInt());
        lobby.players = o.value("players").toInt();
        lobby.maxPlayers = o.value("max").toInt();
        lobby.inGame = o.value("inGame").toBool();
        lobby.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
        bool ok = false;
        const quint32 ipv4 = datagram.senderAddress().toIPv4Address(&ok);
        lobby.address = ok ? QHostAddress(ipv4) : datagram.senderAddress();
        if (lobby.lobbyId.isEmpty() || lobby.port == 0)
            continue;

        auto it = std::find_if(m_lobbies.begin(), m_lobbies.end(),
                               [&](const LobbyAnnouncement &l) { return l.lobbyId == lobby.lobbyId; });
        if (it == m_lobbies.end()) {
            m_lobbies.append(lobby);
            changed = true;
            continue;
        }
        // Une même annonce peut arriver par plusieurs chemins : on préfère
        // l'adresse du réseau local à l'adresse de bouclage.
        if (!it->address.isLoopback() && lobby.address.isLoopback())
            lobby.address = it->address;
        changed = changed || it->players != lobby.players || it->maxPlayers != lobby.maxPlayers
               || it->inGame != lobby.inGame || it->name != lobby.name
               || it->stadiumId != lobby.stadiumId || it->address != lobby.address;
        *it = lobby;
    }
    if (changed)
        emit lobbiesChanged();
}

void LobbyBrowser::expire()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto before = m_lobbies.size();
    m_lobbies.erase(std::remove_if(m_lobbies.begin(), m_lobbies.end(),
                                   [now](const LobbyAnnouncement &l) {
                                       return now - l.lastSeenMs > kLobbyLifetimeMs;
                                   }),
                    m_lobbies.end());
    if (m_lobbies.size() != before)
        emit lobbiesChanged();
}

} // namespace toupie::net
