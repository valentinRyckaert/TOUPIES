#include "net/LobbyServer.h"

#include "core/PartCatalog.h"
#include "game/Battle.h"
#include "game/Stadium.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QUuid>

#include <algorithm>

namespace toupie::net {

namespace {
constexpr int kHelloTimeoutMs = 5000;
constexpr int kAnnounceIntervalMs = 1000;
} // namespace

LobbyServer::LobbyServer(const PartCatalog &catalog, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_fingerprint(catalogFingerprint(catalog))
{
    m_announceTimer.setInterval(kAnnounceIntervalMs);
    connect(&m_announceTimer, &QTimer::timeout, this, &LobbyServer::announce);
}

LobbyServer::~LobbyServer()
{
    close();
}

bool LobbyServer::start(const Settings &settings, quint16 port, quint16 discoveryPort)
{
    close();
    m_server = new QTcpServer(this);
    bool listening = false;
    if (port == 0) {
        listening = m_server->listen(QHostAddress::Any, 0);
    } else {
        for (int i = 0; i < kPortAttempts && !listening; ++i)
            listening = m_server->listen(QHostAddress::Any, static_cast<quint16>(port + i));
    }
    if (!listening) {
        m_error = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QTcpServer::newConnection, this, &LobbyServer::onNewConnection);

    m_lobby = LobbyInfo{};
    m_lobby.lobbyId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    m_lobby.name = sanitizeName(settings.lobbyName, QStringLiteral("Lobby"));
    m_lobby.maxPlayers = std::clamp(settings.maxPlayers, Battle::kMinPlayers, Battle::kMaxPlayers);
    m_lobby.stadiumId = Stadium::find(settings.stadiumId) ? settings.stadiumId
                                                          : Stadium::classic().id;
    m_lobby.tournament = settings.tournament;
    if (m_lobby.tournament)
        m_lobby.tournament->fitBots(m_lobby.maxPlayers);
    LobbyPlayer host;
    host.id = kHostId;
    host.name = sanitizeName(settings.hostName, QStringLiteral("Hôte"));
    host.host = true;
    host.ready = true;
    m_lobby.players.append(host);
    m_nextId = 1;

    m_discoveryPort = discoveryPort;
    if (m_discoveryPort != 0) {
        m_udp = new QUdpSocket(this);
        m_udp->bind(QHostAddress::AnyIPv4, 0); // Socket IPv4 explicite pour les diffusions.
        announce();
        m_announceTimer.start();
    }
    emit lobbyChanged();
    return true;
}

void LobbyServer::close(const QString &reason)
{
    m_announceTimer.stop();
    delete m_udp;
    m_udp = nullptr;
    for (Client &client : m_clients) {
        if (client.id >= 0)
            client.channel->send({{"type", Msg::Closed}, {"reason", reason}});
        disconnect(client.channel, nullptr, this, nullptr);
        // Laisse le temps au dernier message de partir avant de détruire le canal.
        JsonChannel *channel = client.channel;
        connect(channel, &JsonChannel::disconnected, channel, &QObject::deleteLater);
        QTimer::singleShot(2000, channel, &QObject::deleteLater);
        channel->close();
    }
    m_clients.clear();
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

bool LobbyServer::isListening() const
{
    return m_server && m_server->isListening();
}

quint16 LobbyServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

void LobbyServer::setHostTops(const QList<Top> &tops)
{
    if (LobbyPlayer *host = m_lobby.player(kHostId)) {
        host->tops = tops.mid(0, 3);
        lobbyUpdated();
    }
}

void LobbyServer::setTournamentSettings(TournamentSettings settings)
{
    if (!m_lobby.tournament)
        return;
    settings.participants = std::clamp(settings.participants, Tournament::kMinParticipants,
                                       Tournament::kMaxParticipants);
    settings.fitBots(m_lobby.maxPlayers);
    if (settings != *m_lobby.tournament) {
        m_lobby.tournament = settings;
        lobbyUpdated();
    }
}

void LobbyServer::setMaxPlayers(int maxPlayers)
{
    const int minimum = std::max(Battle::kMinPlayers, static_cast<int>(m_lobby.players.size()));
    const int value = std::clamp(maxPlayers, minimum, Battle::kMaxPlayers);
    if (value != m_lobby.maxPlayers) {
        m_lobby.maxPlayers = value;
        if (m_lobby.tournament)
            m_lobby.tournament->fitBots(value);
        lobbyUpdated();
    }
}

void LobbyServer::setStadium(const QString &stadiumId)
{
    if (Stadium::find(stadiumId) && stadiumId != m_lobby.stadiumId) {
        m_lobby.stadiumId = stadiumId;
        lobbyUpdated();
    }
}

void LobbyServer::setRules(const MatchRules &rules)
{
    if (rules != m_lobby.rules) {
        m_lobby.rules = rules;
        lobbyUpdated();
    }
}

bool LobbyServer::canStart() const
{
    if (m_lobby.inGame || m_lobby.players.size() != m_lobby.maxPlayers)
        return false;
    const int needed = m_lobby.topsNeeded();
    return std::all_of(m_lobby.players.begin(), m_lobby.players.end(), [&](const LobbyPlayer &p) {
        return p.tops.size() >= needed && (p.host || p.ready);
    });
}

QJsonObject LobbyServer::startMatch(quint32 seed)
{
    m_lobby.inGame = true;
    QJsonArray players;
    for (const LobbyPlayer &p : m_lobby.players) {
        players.append(QJsonObject{
            {"id", p.id},
            {"name", p.name},
            {"top", p.tops.isEmpty() ? QJsonObject{} : p.tops.first().toJson()},
        });
    }
    const QJsonObject start{
        {"type", Msg::Start},
        {"stadium", m_lobby.stadiumId},
        {"seed", static_cast<qint64>(seed)},
        {"rules", m_lobby.rules.toJson()},
        {"players", players},
    };
    broadcast(start);
    lobbyUpdated();
    return start;
}

void LobbyServer::startTournament()
{
    m_lobby.inGame = true;
    lobbyUpdated();
}

void LobbyServer::returnToLobby()
{
    m_lobby.inGame = false;
    for (LobbyPlayer &p : m_lobby.players) {
        if (!p.host)
            p.ready = false;
    }
    lobbyUpdated();
}

void LobbyServer::sendTo(int playerId, const QJsonObject &message)
{
    for (Client &client : m_clients) {
        if (client.id == playerId)
            client.channel->send(message);
    }
}

void LobbyServer::broadcast(const QJsonObject &message)
{
    for (Client &client : m_clients) {
        if (client.id >= 0)
            client.channel->send(message);
    }
}

// ---------------------------------------------------------------------------

void LobbyServer::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        auto *channel = new JsonChannel(socket, this);
        m_clients.append({-1, channel});
        connect(channel, &JsonChannel::messageReceived, this,
                [this, channel](const QJsonObject &message) { onMessage(channel, message); });
        connect(channel, &JsonChannel::disconnected, this,
                [this, channel] { onDisconnected(channel); });
        // Un client qui ne se présente pas est déconnecté.
        QTimer::singleShot(kHelloTimeoutMs, channel, [this, channel] {
            const Client *client = clientFor(channel);
            if (client && client->id < 0)
                reject(channel, tr("Délai de connexion dépassé."));
        });
    }
}

LobbyServer::Client *LobbyServer::clientFor(JsonChannel *channel)
{
    for (Client &client : m_clients) {
        if (client.channel == channel)
            return &client;
    }
    return nullptr;
}

void LobbyServer::reject(JsonChannel *channel, const QString &reason)
{
    channel->send({{"type", Msg::Reject}, {"reason", reason}});
    channel->close();
}

void LobbyServer::onMessage(JsonChannel *channel, const QJsonObject &message)
{
    Client *client = clientFor(channel);
    if (!client)
        return;
    const QString type = message.value("type").toString();

    if (client->id < 0) {
        if (type == Msg::Hello) {
            handleHello(*client, message);
        } else if (type == Msg::Probe) {
            // Découverte par balayage TCP : description du lobby puis fermeture.
            QJsonObject info = announcementJson();
            info.insert("type", Msg::Info);
            channel->send(info);
            channel->close();
        }
        return;
    }

    LobbyPlayer *player = m_lobby.player(client->id);
    if (!player)
        return;

    if (type == Msg::SelectTop) {
        if (m_lobby.inGame)
            return;
        QList<Top> tops;
        for (const QJsonValue &value : message.value("tops").toArray()) {
            if (auto top = Top::fromJson(value.toObject(), m_catalog); top && tops.size() < 3)
                tops.append(*top);
        }
        player->tops = tops;
        player->ready = false;
        lobbyUpdated();
    } else if (type == Msg::Ready) {
        if (m_lobby.inGame || player->tops.size() < m_lobby.topsNeeded())
            return;
        player->ready = message.value("ready").toBool();
        lobbyUpdated();
    } else if (type == Msg::Aim || type == Msg::Dash || type == Msg::Special
               || type == Msg::Steer || type == Msg::Duel || type == Msg::Order
               || type == Msg::Forfeit) {
        if (m_lobby.inGame)
            emit gameMessage(client->id, message);
    }
}

void LobbyServer::handleHello(Client &client, const QJsonObject &message)
{
    if (message.value("version").toInt() != kProtocolVersion
        || message.value("app").toString() != kApplicationTag) {
        reject(client.channel, tr("Version du jeu incompatible avec celle de l'hôte."));
        return;
    }
    if (message.value("catalog").toString() != m_fingerprint) {
        reject(client.channel, tr("Catalogue de pièces différent de celui de l'hôte."));
        return;
    }
    if (m_lobby.inGame) {
        reject(client.channel, tr("Une partie est déjà en cours dans ce lobby."));
        return;
    }
    if (m_lobby.players.size() >= m_lobby.maxPlayers) {
        reject(client.channel, tr("Le lobby est complet (%1 joueurs).").arg(m_lobby.maxPlayers));
        return;
    }

    LobbyPlayer player;
    player.id = m_nextId++;
    player.name = uniqueName(sanitizeName(message.value("name").toString(),
                                          tr("Joueur %1").arg(player.id + 1)));
    client.id = player.id;
    m_lobby.players.append(player);

    client.channel->send({{"type", Msg::Welcome}, {"playerId", player.id}});
    lobbyUpdated();
}

void LobbyServer::onDisconnected(JsonChannel *channel)
{
    const Client *client = clientFor(channel);
    if (!client)
        return;
    const int id = client->id;
    m_clients.erase(std::remove_if(m_clients.begin(), m_clients.end(),
                                   [channel](const Client &c) { return c.channel == channel; }),
                    m_clients.end());
    channel->deleteLater();

    if (id >= 0) {
        m_lobby.players.erase(std::remove_if(m_lobby.players.begin(), m_lobby.players.end(),
                                             [id](const LobbyPlayer &p) { return p.id == id; }),
                              m_lobby.players.end());
        emit playerLeft(id);
        lobbyUpdated();
    }
}

QString LobbyServer::uniqueName(const QString &name) const
{
    auto taken = [this](const QString &candidate) {
        return std::any_of(m_lobby.players.begin(), m_lobby.players.end(),
                           [&](const LobbyPlayer &p) {
                               return p.name.compare(candidate, Qt::CaseInsensitive) == 0;
                           });
    };
    QString candidate = name;
    for (int i = 2; taken(candidate); ++i)
        candidate = QStringLiteral("%1 (%2)").arg(name).arg(i);
    return candidate;
}

void LobbyServer::lobbyUpdated()
{
    broadcast({{"type", Msg::Lobby}, {"lobby", toJson(m_lobby)}});
    emit lobbyChanged();
    announce();
}

QJsonObject LobbyServer::announcementJson() const
{
    const LobbyPlayer *host = m_lobby.player(kHostId);
    return {
        {"app", kApplicationTag},
        {"v", kProtocolVersion},
        {"id", m_lobby.lobbyId},
        {"name", m_lobby.name},
        {"host", host ? host->name : QString()},
        {"port", m_server ? m_server->serverPort() : 0},
        {"players", static_cast<int>(m_lobby.players.size())},
        {"max", m_lobby.maxPlayers},
        {"stadium", m_lobby.stadiumId},
        {"inGame", m_lobby.inGame},
        {"tournament", m_lobby.tournament.has_value()},
    };
}

void LobbyServer::announce()
{
    if (!m_udp || !m_server)
        return;

    const QByteArray datagram = QJsonDocument(announcementJson()).toJson(QJsonDocument::Compact);

    // Diffusion générale, diffusion dirigée sur chaque interface et boucle
    // locale (plusieurs instances sur une même machine).
    QList<QHostAddress> targets{QHostAddress(QHostAddress::Broadcast),
                                QHostAddress(QHostAddress::LocalHost)};
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::CanBroadcast)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (!entry.broadcast().isNull() && !targets.contains(entry.broadcast()))
                targets.append(entry.broadcast());
        }
    }
    for (const QHostAddress &target : targets)
        m_udp->writeDatagram(datagram, target, m_discoveryPort);
}

} // namespace toupie::net
