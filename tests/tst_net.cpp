#include "core/PartCatalog.h"
#include "net/ClientSession.h"
#include "net/HostSession.h"
#include "net/LobbyBrowser.h"
#include "net/MatchCodec.h"
#include "net/LobbyClient.h"
#include "net/LobbyScanner.h"
#include "net/LobbyServer.h"
#include "net/Protocol.h"

#include <QSignalSpy>

#include <cmath>
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
    void lobbyIsFoundByTcpScan();
    void localScanTargetsExcludeVirtualInterfaces();
    void stateCodecRoundTrip();
    void networkMatchRunsEndToEnd();
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
    QVERIFY(browser.start(false));

    LobbyServer server(PartCatalog::instance());
    QVERIFY(server.start(settings(3), 0, kTestDiscoveryPort));

    QTRY_VERIFY_WITH_TIMEOUT(browser.findByName(QStringLiteral("salon TEST")).has_value(), 4000);
    const LobbyAnnouncement lobby = *browser.findByName(QStringLiteral("Salon test"));
    QCOMPARE(lobby.port, server.port());
    QCOMPARE(lobby.maxPlayers, 3);
    QCOMPARE(lobby.players, 1);
    QCOMPARE(lobby.stadiumId, QStringLiteral("glace"));
}

void TestNet::lobbyIsFoundByTcpScan()
{
    // Découverte de secours quand l'UDP est filtré : sonde TCP « probe ».
    LobbyServer server(PartCatalog::instance());
    QVERIFY(server.start(settings(4), 0, 0)); // Aucune annonce UDP.

    LobbyScanner scanner;
    scanner.setPorts({static_cast<quint16>(server.port() + 1), server.port()});
    QSignalSpy found(&scanner, &LobbyScanner::lobbyFound);
    QSignalSpy finished(&scanner, &LobbyScanner::finished);
    scanner.scan({QHostAddress(QHostAddress::LocalHost)});
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(found.count(), 1);
    const auto lobby = found.first().first().value<LobbyAnnouncement>();
    QCOMPARE(lobby.name, QStringLiteral("Salon test"));
    QCOMPARE(lobby.port, server.port());
    QCOMPARE(lobby.maxPlayers, 4);
    QVERIFY(lobby.address.isLoopback());

    // La sonde n'occupe pas de place dans le lobby.
    QCOMPARE(server.lobby().players.size(), 1);
}

void TestNet::localScanTargetsExcludeVirtualInterfaces()
{
    const QList<QHostAddress> targets = LobbyScanner::localTargets();
    QVERIFY(targets.size() <= 4 * 4094);
    for (const QHostAddress &address : targets) {
        QVERIFY(!address.isLoopback());
        QCOMPARE(address.protocol(), QAbstractSocket::IPv4Protocol);
    }
}

void TestNet::stateCodecRoundTrip()
{
    const PartCatalog &catalog = PartCatalog::instance();
    const QList<Top> tops{catalog.presets().at(0), catalog.presets().at(1), catalog.presets().at(2)};
    Battle battle(tops, *Stadium::find(QStringLiteral("beton")), BattleConfig{}, 5);
    battle.launch({0.0, 1.0, 2.0});
    battle.advance(1.0);

    MatchState source;
    source.spinners = battle.spinners();
    source.aimed = {true, true, true};
    source.phase = MatchPhase::Fighting;
    source.elapsed = battle.elapsed();
    source.ranking = {2, 0, 1};
    source.winner = 2;
    ImpactEvent impact;
    impact.point = {0.25, -0.5};
    impact.intensity = 0.75;

    MatchState target;
    target.spinners = std::vector<Spinner>(3);
    target.aimed = {false, false, false};
    QList<ImpactEvent> impacts;
    QVERIFY(decodeState(encodeState(source, {impact}), target, &impacts));
    QCOMPARE(target.phase, MatchPhase::Fighting);
    QCOMPARE(target.winner, 2);
    QCOMPARE(target.ranking, source.ranking);
    QCOMPARE(impacts.size(), 1);
    QCOMPARE(impacts.first().intensity, 0.75);
    for (int i = 0; i < 3; ++i) {
        QVERIFY(std::abs(target.spinners[i].position.x - source.spinners[i].position.x) < 1e-3);
        QVERIFY(std::abs(target.spinners[i].spin - source.spinners[i].spin) < 1e-3);
        QVERIFY(target.spinners[i].status == source.spinners[i].status);
    }

    // Nombre de toupies incohérent : message ignoré.
    MatchState wrong;
    wrong.spinners = std::vector<Spinner>(2);
    QVERIFY(!decodeState(encodeState(source, {}), wrong, nullptr));
}

void TestNet::networkMatchRunsEndToEnd()
{
    const PartCatalog &catalog = PartCatalog::instance();
    LobbyServer server(catalog);
    QVERIFY(server.start(settings(), 0, 0));
    auto *client = new LobbyClient(catalog);
    QSignalSpy started(client, &LobbyClient::matchStarted);
    client->connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("Alice"));
    QTRY_VERIFY(client->isJoined());
    server.setHostTop(catalog.presets().at(1));
    client->selectTop(catalog.presets().at(3));
    client->setReady(true);
    QTRY_VERIFY(server.canStart());

    const auto hostSetup = parseStart(server.startMatch(7), catalog);
    QVERIFY(hostSetup.has_value());
    HostSession host(&server, *hostSetup);
    QTRY_COMPARE(started.count(), 1);
    const auto clientSetup = parseStart(started.first().first().toJsonObject(), catalog);
    QVERIFY(clientSetup.has_value());
    ClientSession guest(client, *clientSetup);

    QCOMPARE(host.localPlayer(), 0);
    QCOMPARE(guest.localPlayer(), 1);
    QCOMPARE(guest.state().stadium.id, QStringLiteral("glace"));

    auto pump = [&](double seconds) {
        for (double t = 0; t < seconds; t += 1.0 / 60.0) {
            host.update(1.0 / 60.0);
            guest.update(1.0 / 60.0);
            QTest::qWait(2);
        }
    };

    host.submitAim(0.0, 1.0);
    pump(0.2);
    QCOMPARE(host.state().phase, MatchPhase::Aiming); // Le client n'a pas visé.
    QTRY_VERIFY(guest.state().aimed.value(0));
    guest.submitAim(3.14, 1.2);
    QTRY_VERIFY_WITH_TIMEOUT((pump(0.05), host.state().phase == MatchPhase::Countdown), 3000);
    QTRY_VERIFY_WITH_TIMEOUT((pump(0.05), guest.state().phase == MatchPhase::Fighting), 6000);
    QVERIFY(host.state().phase == MatchPhase::Fighting
            || host.state().phase == MatchPhase::Finished);

    // Le dash du client est appliqué par l'hôte (s'il est encore en jeu).
    if (host.state().phase == MatchPhase::Fighting && host.state().spinners[1].isActive()) {
        guest.requestDash();
        QTRY_VERIFY_WITH_TIMEOUT((pump(0.02), host.state().spinners[1].dashCooldown > 0.0
                                                  || host.state().phase == MatchPhase::Finished),
                                 2000);
    }

    // Le client se déconnecte : l'hôte gagne par abandon s'il restait en jeu.
    const bool stillRunning = host.state().phase == MatchPhase::Fighting;
    delete client;
    QTRY_VERIFY_WITH_TIMEOUT((pump(0.05), host.state().phase == MatchPhase::Finished), 3000);
    if (stillRunning) {
        QCOMPARE(host.state().winner, 0);
        QVERIFY(host.state().endReason == Battle::EndReason::Forfeit);
    }
}

QTEST_GUILESS_MAIN(TestNet)
#include "tst_net.moc"
