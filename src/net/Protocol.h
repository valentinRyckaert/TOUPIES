#pragma once

#include "core/Top.h"
#include "game/MatchRules.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>

class QTcpSocket;

namespace toupie {

class PartCatalog;

/// Protocole réseau du mode multijoueur local.
///
/// - Découverte : l'hôte diffuse toutes les secondes une annonce JSON en UDP
///   (port kDiscoveryPort) ; les clients écoutent et listent les lobbys. En
///   secours (UDP filtré), les clients sondent les sous-réseaux locaux en TCP
///   avec un message « probe » auquel l'hôte répond par « info ».
/// - Lobby et partie : connexion TCP vers l'hôte, un message JSON compact
///   par ligne. Chaque message possède un champ "type".
/// - L'hôte fait autorité : il simule le combat et diffuse l'état ; les
///   clients n'envoient que leur visée (angle et force), leur pilotage,
///   leurs dashs et leurs coups spéciaux.
namespace net {

constexpr int kProtocolVersion = 3;
constexpr quint16 kDefaultTcpPort = 45455;
constexpr quint16 kDiscoveryPort = 45454;
constexpr int kPortAttempts = 10;           ///< Ports essayés à partir du port par défaut.
constexpr int kMaxNameLength = 20;
constexpr double kStateRate = 30.0;         ///< Envois d'état par seconde.
constexpr double kAimTimeout = 30.0;        ///< Visée automatique au-delà (secondes).
const QString kApplicationTag = QStringLiteral("ToupieArena");

namespace Msg {
// Client -> hôte
const QString Hello = QStringLiteral("hello");
const QString Probe = QStringLiteral("probe");
const QString SelectTop = QStringLiteral("top");
const QString Ready = QStringLiteral("ready");
const QString Aim = QStringLiteral("aim");
const QString Dash = QStringLiteral("dash");
const QString Special = QStringLiteral("special");
const QString Steer = QStringLiteral("steer");
// Hôte -> client
const QString Welcome = QStringLiteral("welcome");
const QString Info = QStringLiteral("info");
const QString Reject = QStringLiteral("reject");
const QString Lobby = QStringLiteral("lobby");
const QString Start = QStringLiteral("start");
const QString State = QStringLiteral("state");
const QString Closed = QStringLiteral("closed");
} // namespace Msg

/// Empreinte du catalogue de pièces : deux joueurs doivent avoir le même
/// catalogue pour échanger des toupies par identifiants de pièces.
QString catalogFingerprint(const PartCatalog &catalog);

/// Nettoie un pseudo ou un nom de lobby (espaces, longueur).
QString sanitizeName(const QString &name, const QString &fallback);

struct LobbyPlayer
{
    int id = -1;
    QString name;
    std::optional<Top> top;
    bool ready = false;
    bool host = false;
};

struct LobbyInfo
{
    QString lobbyId;
    QString name;
    int maxPlayers = 2;
    QString stadiumId;
    bool inGame = false;
    MatchRules rules;
    QList<LobbyPlayer> players;

    const LobbyPlayer *player(int id) const;
    LobbyPlayer *player(int id);
};

QJsonObject toJson(const LobbyInfo &lobby);
LobbyInfo lobbyFromJson(const QJsonObject &json, const PartCatalog &catalog);

/// Lobby découvert sur le réseau.
struct LobbyAnnouncement
{
    QString lobbyId;
    QString name;
    QString hostName;
    QString stadiumId;
    QHostAddress address;
    quint16 port = 0;
    int players = 0;
    int maxPlayers = 0;
    bool inGame = false;
    qint64 lastSeenMs = 0;
};

/// Canal de messages JSON (un par ligne) au-dessus d'une socket TCP.
class JsonChannel : public QObject
{
    Q_OBJECT

public:
    /// Prend possession de la socket.
    explicit JsonChannel(QTcpSocket *socket, QObject *parent = nullptr);

    QTcpSocket *socket() const { return m_socket; }
    void send(const QJsonObject &message);
    void close();

signals:
    void messageReceived(const QJsonObject &message);
    void disconnected();

private:
    void readAvailable();

    QTcpSocket *m_socket;
    QByteArray m_buffer;
};

} // namespace net
} // namespace toupie

Q_DECLARE_METATYPE(toupie::net::LobbyAnnouncement)
