#include "net/Protocol.h"

#include "core/PartCatalog.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpSocket>

namespace toupie::net {

namespace {
constexpr qsizetype kMaxLineLength = 1 << 20; // Protection contre les messages aberrants.
}

QString catalogFingerprint(const PartCatalog &catalog)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (const Part &part : catalog.parts()) {
        hash.addData(QStringLiteral("%1:%2:%3:%4:%5:%6;")
                         .arg(part.id)
                         .arg(static_cast<int>(part.kind))
                         .arg(part.stats.attack)
                         .arg(part.stats.defense)
                         .arg(part.stats.endurance)
                         .arg(static_cast<int>(part.spin))
                         .toUtf8());
    }
    return QString::fromLatin1(hash.result().toHex().left(12));
}

QString sanitizeName(const QString &name, const QString &fallback)
{
    QString cleaned = name.simplified();
    cleaned.remove(QChar('\n'));
    cleaned = cleaned.left(kMaxNameLength).trimmed();
    return cleaned.isEmpty() ? fallback : cleaned;
}

const LobbyPlayer *LobbyInfo::player(int id) const
{
    for (const LobbyPlayer &p : players) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

LobbyPlayer *LobbyInfo::player(int id)
{
    for (LobbyPlayer &p : players) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

QJsonObject toJson(const LobbyInfo &lobby)
{
    QJsonArray players;
    for (const LobbyPlayer &p : lobby.players) {
        QJsonObject player{
            {"id", p.id},
            {"name", p.name},
            {"ready", p.ready},
            {"host", p.host},
        };
        if (p.top)
            player.insert("top", p.top->toJson());
        players.append(player);
    }
    return {
        {"id", lobby.lobbyId},
        {"name", lobby.name},
        {"max", lobby.maxPlayers},
        {"stadium", lobby.stadiumId},
        {"inGame", lobby.inGame},
        {"players", players},
    };
}

LobbyInfo lobbyFromJson(const QJsonObject &json, const PartCatalog &catalog)
{
    LobbyInfo lobby;
    lobby.lobbyId = json.value("id").toString();
    lobby.name = json.value("name").toString();
    lobby.maxPlayers = json.value("max").toInt(2);
    lobby.stadiumId = json.value("stadium").toString();
    lobby.inGame = json.value("inGame").toBool();
    const QJsonArray players = json.value("players").toArray();
    for (const QJsonValue &value : players) {
        const QJsonObject o = value.toObject();
        LobbyPlayer p;
        p.id = o.value("id").toInt(-1);
        p.name = o.value("name").toString();
        p.ready = o.value("ready").toBool();
        p.host = o.value("host").toBool();
        if (o.contains("top"))
            p.top = Top::fromJson(o.value("top").toObject(), catalog);
        lobby.players.append(p);
    }
    return lobby;
}

// ---------------------------------------------------------------------------

JsonChannel::JsonChannel(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(m_socket, &QTcpSocket::readyRead, this, &JsonChannel::readAvailable);
    connect(m_socket, &QTcpSocket::disconnected, this, &JsonChannel::disconnected);
}

void JsonChannel::send(const QJsonObject &message)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact);
    data.append('\n');
    m_socket->write(data);
}

void JsonChannel::close()
{
    m_socket->flush();
    m_socket->disconnectFromHost();
}

void JsonChannel::readAvailable()
{
    m_buffer.append(m_socket->readAll());
    qsizetype newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject())
            emit messageReceived(doc.object());
    }
    if (m_buffer.size() > kMaxLineLength) {
        m_buffer.clear();
        m_socket->abort();
    }
}

} // namespace toupie::net
