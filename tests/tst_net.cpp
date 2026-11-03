#include "core/PartCatalog.h"
#include "net/LobbyBrowser.h"
#include "net/LobbyClient.h"
#include "net/LobbyServer.h"
#include "net/Protocol.h"

#include <QSignalSpy>
#include <QJsonArray>
#include <QTest>

using namespace toupie;
using namespace toupie::net;

namespace {
constexpr quint16 kTestDiscoveryPort = 45987;

LobbyServer::Settings settings(int maxPlayers = 2)
{
    return {QStringLiteral("Salon test"), QStringLiteral("Hôte"), maxPlayers,
            QStringLiteral("glace")};
}
} // namespace

class TestNet : public QObject
{
    Q_OBJECT

private slots:
    void sanitizeNames();
    void lobbyJsonRoundTrip();
    void clientJoinsChoosesTopAndStarts();
    void fullLobbyRejectsPlayer();
    void differentCatalogIsRejected();
    void serverNoticesDisconnection();
    void gameMessagesAreRelayedDuringMatch();
    void lobbyIsDiscoveredByName();
};

void TestNet::sanitizeNames()
{
    QCOMPARE(sanitizeName(QStringLiteral("  Alice   B  "), QStringLiteral("x")),
             QStringLiteral("Alice B"));
    QCOMPARE(sanitizeName(QStringLiteral("   "), QStringLiteral("Joueur")), QStringLiteral("Joueur"));
    QCOMPARE(sanitizeName(QString(50, 'a'), QStringLiteral("x")).size(), kMaxNameLength);
}

void TestNet::lobbyJsonRoundTrip()
{
    const PartCatalog &catalog = PartCatalog::instance();
    LobbyInfo lobby;
    lobby.lobbyId = QStringLiteral("abc");
    lobby.name = QStringLiteral("Salon");
    lobby.maxPlayers = 3;
    lobby.stadiumId = QStringLiteral("terre");
    LobbyPlayer host{0, QStringLiteral("Hôte"), catalog.presets().first(), true, true};
    LobbyPlayer guest{1, QStringLiteral("Invité"), std::nullopt, false, false};
    lobby.players = {host, guest};

    const LobbyInfo copy = lobbyFromJson(toJson(lobby), catalog);
    QCOMPARE(copy.name, lobby.name);
    QCOMPARE(copy.maxPlayers, 3);
    QCOMPARE(copy.stadiumId, lobby.stadiumId);
    QCOMPARE(copy.players.size(), 2);
    QVERIFY(copy.players[0].top.has_value());
    QCOMPARE(copy.players[0].top->name(), host.top->name());
    QVERIFY(!copy.players[1].top.has_value());
    QVERIFY(copy.players[0].host);
}

void TestNet::clientJoinsChoosesTopAndStarts()
{
    const PartCatalog &catalog = PartCatalog::instance();
    LobbyServer server(catalog);
    QVERIFY(server.start(settings(), 0, 0));

    LobbyClient client(catalog);
    QSignalSpy started(&client, &LobbyClient::matchStarted);
    client.connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("Alice"));
    QTRY_VERIFY(client.isJoined());
    QTRY_COMPARE(client.lobby().players.size(), 2);
    QCOMPARE(client.lobby().stadiumId, QStringLiteral("glace"));

    server.setHostTop(catalog.presets().at(0));
    QVERIFY(!server.canStart()); // Le client n'a pas encore choisi.
    client.selectTop(catalog.presets().at(2));
    client.setReady(true);
    QTRY_VERIFY(server.canStart());
    QCOMPARE(server.lobby().players.at(1).name, QStringLiteral("Alice"));
    QCOMPARE(server.lobby().players.at(1).top->name(), catalog.presets().at(2).name());

    const QJsonObject start = server.startMatch(42);
    QCOMPARE(start.value("players").toArray().size(), 2);
    QTRY_COMPARE(started.count(), 1);
    QTRY_VERIFY(client.lobby().inGame);

    server.returnToLobby();
    QTRY_VERIFY(!client.lobby().inGame);
    QVERIFY(!server.lobby().players.at(1).ready);
}

void TestNet::fullLobbyRejectsPlayer()
{
    const PartCatalog &catalog = PartCatalog::instance();
    LobbyServer server(catalog);
    QVERIFY(server.start(settings(2), 0, 0));

    LobbyClient first(catalog);
    first.connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("A"));
    QTRY_VERIFY(first.isJoined());

    LobbyClient second(catalog);
    QSignalSpy failed(&second, &LobbyClient::joinFailed);
    second.connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("B"));
    QTRY_COMPARE(failed.count(), 1);
    QVERIFY(failed.first().first().toString().contains(QStringLiteral("complet")));
    QVERIFY(!second.isJoined());

    // Le nombre de joueurs ne peut pas descendre sous le nombre de connectés.
    server.setMaxPlayers(4);
    QCOMPARE(server.lobby().maxPlayers, 4);
}

void TestNet::differentCatalogIsRejected()
{
    LobbyServer server(PartCatalog::instance());
    QVERIFY(server.start(settings(), 0, 0));

    const PartCatalog other(PartCatalog::instance().parts().mid(1));
    LobbyClient client(other);
    QSignalSpy failed(&client, &LobbyClient::joinFailed);
    client.connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("A"));
    QTRY_COMPARE(failed.count(), 1);
}

void TestNet::serverNoticesDisconnection()
{
    const PartCatalog &catalog = PartCatalog::instance();
    LobbyServer server(catalog);
    QVERIFY(server.start(settings(3), 0, 0));
    QSignalSpy left(&server, &LobbyServer::playerLeft);

    auto *client = new LobbyClient(catalog);
    client->connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("A"));
    QTRY_VERIFY(client->isJoined());
    QTRY_COMPARE(server.lobby().players.size(), 2);
    delete client;
    QTRY_COMPARE(left.count(), 1);
    QCOMPARE(server.lobby().players.size(), 1);

    // Et un client voit la fermeture du lobby par l'hôte.
    LobbyClient other(catalog);
    QSignalSpy lost(&other, &LobbyClient::connectionLost);
    other.connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("B"));
    QTRY_VERIFY(other.isJoined());
    server.close(QStringLiteral("Fin de soirée"));
    QTRY_COMPARE(lost.count(), 1);
    QCOMPARE(lost.first().first().toString(), QStringLiteral("Fin de soirée"));
}

void TestNet::gameMessagesAreRelayedDuringMatch()
{
    const PartCatalog &catalog = PartCatalog::instance();
    LobbyServer server(catalog);
    QVERIFY(server.start(settings(), 0, 0));
    QSignalSpy relayed(&server, &LobbyServer::gameMessage);

    LobbyClient client(catalog);
    client.connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("A"));
    QTRY_VERIFY(client.isJoined());

    // Hors partie, les messages de jeu sont ignorés.
    client.send({{"type", Msg::Dash}});
    QTest::qWait(100);
    QCOMPARE(relayed.count(), 0);

    server.setHostTop(catalog.presets().at(0));
    client.selectTop(catalog.presets().at(1));
    client.setReady(true);
    QTRY_VERIFY(server.canStart());
    server.startMatch(1);
    client.send({{"type", Msg::Aim}, {"angle", 1.5}});
    QTRY_COMPARE(relayed.count(), 1);
    QCOMPARE(relayed.first().at(0).toInt(), client.playerId());
    QCOMPARE(relayed.first().at(1).toJsonObject().value("angle").toDouble(), 1.5);
}

void TestNet::lobbyIsDiscoveredByName()
{
    LobbyBrowser browser(kTestDiscoveryPort);
    QVERIFY(browser.start());

    LobbyServer server(PartCatalog::instance());
    QVERIFY(server.start(settings(3), 0, kTestDiscoveryPort));

    QTRY_VERIFY_WITH_TIMEOUT(browser.findByName(QStringLiteral("salon TEST")).has_value(), 4000);
    const LobbyAnnouncement lobby = *browser.findByName(QStringLiteral("Salon test"));
    QCOMPARE(lobby.port, server.port());
    QCOMPARE(lobby.maxPlayers, 3);
    QCOMPARE(lobby.players, 1);
    QCOMPARE(lobby.stadiumId, QStringLiteral("glace"));
}

QTEST_GUILESS_MAIN(TestNet)
#include "tst_net.moc"
