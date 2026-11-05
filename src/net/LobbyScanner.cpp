#include "net/LobbyScanner.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QSet>
#include <QTcpSocket>
#include <QTimer>
#include <algorithm>

namespace toupie::net {

namespace {
/// Sous la limite de fichiers ouverts par défaut de macOS (256).
constexpr int kConcurrency = 128;
constexpr int kProbeTimeoutMs = 350;
constexpr int kMinPrefix = 20; ///< Au plus 4094 adresses par interface.

bool isVirtualInterface(const QNetworkInterface &iface)
{
    static const QStringList prefixes{QStringLiteral("docker"), QStringLiteral("br-"),
                                      QStringLiteral("virbr"),  QStringLiteral("veth"),
                                      QStringLiteral("vmnet"),  QStringLiteral("vboxnet"),
                                      QStringLiteral("lxc"),    QStringLiteral("podman")};
    // Noms Windows (« vEthernet (WSL) », « VirtualBox Host-Only Network »…) :
    // ces cartes ajoutent des sous-réseaux entiers à sonder pour rien.
    static const QStringList windowsNames{
        QStringLiteral("vethernet"), QStringLiteral("virtualbox"), QStringLiteral("vmware"),
        QStringLiteral("hyper-v"),   QStringLiteral("wsl"),        QStringLiteral("host-only"),
        QStringLiteral("bluetooth"), QStringLiteral("pseudo")};
    const QString name = iface.name().toLower();
    for (const QString &prefix : prefixes) {
        if (name.startsWith(prefix))
            return true;
    }
    const QString label = iface.humanReadableName().toLower();
    for (const QString &part : windowsNames) {
        if (label.contains(part))
            return true;
    }
    return iface.type() == QNetworkInterface::Virtual;
}
} // namespace

LobbyScanner::LobbyScanner(QObject *parent)
    : QObject(parent)
    , m_ports{kDefaultTcpPort, static_cast<quint16>(kDefaultTcpPort + 1),
              static_cast<quint16>(kDefaultTcpPort + 2)}
{
}

LobbyScanner::~LobbyScanner()
{
    cancel();
}

QList<QHostAddress> LobbyScanner::localTargets()
{
    QList<QHostAddress> near;
    QList<QHostAddress> far;
    localTargets(&near, &far);
    return near + far;
}

void LobbyScanner::localTargets(QList<QHostAddress> *nearHosts, QList<QHostAddress> *farHosts)
{
    QList<QHostAddress> &first = *nearHosts;  // /24 de chaque interface
    QList<QHostAddress> &second = *farHosts;  // reste du sous-réseau
    QSet<quint32> seen;

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || (flags & QNetworkInterface::IsLoopBack)
            || isVirtualInterface(iface))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            const int prefix = std::max(kMinPrefix, entry.prefixLength());
            if (prefix >= 31)
                continue;
            const quint32 ip = entry.ip().toIPv4Address();
            const quint32 mask = prefix == 0 ? 0 : 0xFFFFFFFFu << (32 - prefix);
            const quint32 network = ip & mask;
            const quint32 broadcast = network | ~mask;
            const quint32 block24 = ip & 0xFFFFFF00u;
            for (quint32 a = network + 1; a < broadcast; ++a) {
                if (a == ip || seen.contains(a))
                    continue;
                seen.insert(a);
                ((a & 0xFFFFFF00u) == block24 ? first : second).append(QHostAddress(a));
            }
        }
    }
}

void LobbyScanner::scanLocalNetworks(bool nearOnly)
{
    // Proches voisins : tous les ports ; reste du sous-réseau : port principal seulement.
    QList<QHostAddress> nearHosts;
    QList<QHostAddress> farHosts;
    localTargets(&nearHosts, &farHosts);
    cancel();
    for (const QHostAddress &host : nearHosts) {
        for (quint16 port : m_ports)
            m_queue.append({host, port});
    }
    if (!m_ports.isEmpty() && !nearOnly) {
        for (const QHostAddress &host : farHosts)
            m_queue.append({host, m_ports.first()});
    }
    if (m_queue.isEmpty()) {
        emit finished();
        return;
    }
    for (int i = 0; i < kConcurrency; ++i)
        startNext();
}

void LobbyScanner::scan(const QList<QHostAddress> &hosts)
{
    cancel();
    for (const QHostAddress &host : hosts) {
        for (quint16 port : m_ports)
            m_queue.append({host, port});
    }
    if (m_queue.isEmpty()) {
        emit finished();
        return;
    }
    for (int i = 0; i < kConcurrency; ++i)
        startNext();
}

void LobbyScanner::cancel()
{
    m_queue.clear();
    const QList<QTcpSocket *> active = m_active;
    m_active.clear();
    for (QTcpSocket *socket : active) {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
}

void LobbyScanner::startNext()
{
    if (m_queue.isEmpty())
        return;
    const auto [host, port] = m_queue.takeFirst();
    auto *socket = new QTcpSocket(this);
    m_active.append(socket);

    auto *timer = new QTimer(socket);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, socket] { finishProbe(socket); });
    timer->start(kProbeTimeoutMs);

    connect(socket, &QTcpSocket::connected, this, [socket, timer] {
        socket->write(QByteArrayLiteral("{\"type\":\"probe\"}\n"));
        timer->start(kProbeTimeoutMs * 2); // Laisse le temps de répondre.
    });
    connect(socket, &QTcpSocket::readyRead, this, [this, socket, host = host] {
        if (!socket->canReadLine())
            return;
        const QJsonObject info = QJsonDocument::fromJson(socket->readLine()).object();
        if (info.value("type").toString() == QLatin1String("info")
            && info.value("app").toString() == kApplicationTag
            && info.value("v").toInt() == kProtocolVersion) {
            LobbyAnnouncement lobby;
            lobby.lobbyId = info.value("id").toString();
            lobby.name = info.value("name").toString();
            lobby.hostName = info.value("host").toString();
            lobby.stadiumId = info.value("stadium").toString();
            lobby.port = static_cast<quint16>(info.value("port").toInt());
            lobby.players = info.value("players").toInt();
            lobby.maxPlayers = info.value("max").toInt();
            lobby.inGame = info.value("inGame").toBool();
            lobby.tournament = info.value("tournament").toBool();
            lobby.address = host;
            lobby.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
            if (!lobby.lobbyId.isEmpty())
                emit lobbyFound(lobby);
        }
        finishProbe(socket);
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket] { finishProbe(socket); });
    socket->connectToHost(host, port);
}

void LobbyScanner::finishProbe(QTcpSocket *socket)
{
    if (!m_active.removeOne(socket))
        return;
    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
    if (m_queue.isEmpty()) {
        if (m_active.isEmpty())
            emit finished();
        return;
    }
    // Différé : une sonde qui échoue tout de suite (limite de fichiers ouverts,
    // adresse refusée) rappellerait finishProbe en boucle et ferait déborder la
    // pile, petite dans un fil secondaire (512 Ko sous macOS).
    QMetaObject::invokeMethod(this, &LobbyScanner::startNext, Qt::QueuedConnection);
}

} // namespace toupie::net
