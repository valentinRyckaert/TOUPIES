#include "net/LobbyBrowser.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QUdpSocket>

#include <algorithm>

namespace toupie::net {

namespace {
/// Les annonces UDP arrivent chaque seconde, mais le Wi-Fi en perd souvent.
constexpr qint64 kUdpLifetimeMs = 6000;
/// Lobbys trouvés par balayage : conservés jusqu'au balayage suivant.
constexpr qint64 kScanLifetimeMs = 45000;
constexpr int kScanIntervalMs = 30000;
} // namespace

LobbyBrowser::LobbyBrowser(quint16 discoveryPort, QObject *parent)
    : QObject(parent)
    , m_port(discoveryPort)
{
    m_expireTimer.setInterval(1000);
    connect(&m_expireTimer, &QTimer::timeout, this, &LobbyBrowser::expire);
    m_scanTimer.setInterval(kScanIntervalMs);
    connect(&m_scanTimer, &QTimer::timeout, this, &LobbyBrowser::rescan);
    connect(&m_scanner, &LobbyScanner::lobbyFound, this,
            [this](const LobbyAnnouncement &lobby) { merge(lobby, false); });
    connect(&m_scanner, &LobbyScanner::finished, this, &LobbyBrowser::scanFinished);
}

bool LobbyBrowser::start(bool scanNetwork)
{
    if (m_running)
        return m_socket != nullptr;
    m_running = true;
    m_udpReceived = false;
    m_expireTimer.start();

    m_socket = new QUdpSocket(this);
    // Adresse partagée : plusieurs instances du jeu peuvent écouter sur la même machine.
    if (!m_socket->bind(QHostAddress::AnyIPv4, m_port,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        delete m_socket;
        m_socket = nullptr;
    } else {
        connect(m_socket, &QUdpSocket::readyRead, this, &LobbyBrowser::readDatagrams);
    }

    if (scanNetwork) {
        rescan();
        m_scanTimer.start();
    }
    return m_socket != nullptr;
}

void LobbyBrowser::stop()
{
    m_running = false;
    m_expireTimer.stop();
    m_scanTimer.stop();
    m_scanner.cancel();
    delete m_socket;
    m_socket = nullptr;
    if (!m_lobbies.isEmpty()) {
        m_lobbies.clear();
        m_fromUdp.clear();
        emit lobbiesChanged();
    }
}

void LobbyBrowser::rescan()
{
    if (m_running)
        m_scanner.scanLocalNetworks();
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
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram(4096);
        const QJsonObject o = QJsonDocument::fromJson(datagram.data()).object();
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
        lobby.tournament = o.value("tournament").toBool();
        bool ok = false;
        const quint32 ipv4 = datagram.senderAddress().toIPv4Address(&ok);
        lobby.address = ok ? QHostAddress(ipv4) : datagram.senderAddress();
        if (lobby.lobbyId.isEmpty() || lobby.port == 0)
            continue;
        if (!lobby.address.isLoopback())
            m_udpReceived = true;
        merge(lobby, true);
    }
}

void LobbyBrowser::merge(LobbyAnnouncement lobby, bool fromUdp)
{
    lobby.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    auto it = std::find_if(m_lobbies.begin(), m_lobbies.end(),
                           [&](const LobbyAnnouncement &l) { return l.lobbyId == lobby.lobbyId; });
    if (it == m_lobbies.end()) {
        m_lobbies.append(lobby);
        m_fromUdp.append(fromUdp);
        emit lobbiesChanged();
        return;
    }
    const auto index = std::distance(m_lobbies.begin(), it);
    // Une même annonce peut arriver par plusieurs chemins : on préfère
    // l'adresse du réseau local à l'adresse de bouclage.
    if (!it->address.isLoopback() && lobby.address.isLoopback())
        lobby.address = it->address;
    const bool changed = it->players != lobby.players || it->maxPlayers != lobby.maxPlayers
                      || it->inGame != lobby.inGame || it->name != lobby.name
                      || it->stadiumId != lobby.stadiumId || it->address != lobby.address;
    *it = lobby;
    m_fromUdp[index] = fromUdp;
    if (changed)
        emit lobbiesChanged();
}

void LobbyBrowser::expire()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    for (qsizetype i = m_lobbies.size() - 1; i >= 0; --i) {
        const qint64 lifetime = m_fromUdp.at(i) ? kUdpLifetimeMs : kScanLifetimeMs;
        if (now - m_lobbies.at(i).lastSeenMs > lifetime) {
            m_lobbies.removeAt(i);
            m_fromUdp.removeAt(i);
            changed = true;
        }
    }
    if (changed)
        emit lobbiesChanged();
}

} // namespace toupie::net
