#include "core/PartCatalog.h"
#include "net/ClientSession.h"
#include "net/HostSession.h"
#include "net/TournamentSession.h"
#include "net/Updater.h"
#include "net/LobbyBrowser.h"
#include "net/MatchCodec.h"
#include "net/LobbyClient.h"
#include "net/LobbyScanner.h"
#include "net/LobbyServer.h"
#include "net/Protocol.h"

#include <QSignalSpy>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QUdpSocket>

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
            QStringLiteral("glace"), std::nullopt};
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
    void hostAnswersDiscoveryQuery();
    void scanSurvivesImmediateFailuresInThread();
    void localScanTargetsExcludeVirtualInterfaces();
    void stateCodecRoundTrip();
    void networkMatchRunsEndToEnd();
    void duelSeriesAndBotsSurviveCodec();
    void soloTournamentPlaysAndFinishes();
    void lanTournamentStartsMatchesOnClients();
    void updateIsFoundInLatestRelease();
#ifdef Q_OS_LINUX
    void appImageIsReplacedAndRelaunched();
#endif
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
    LobbyPlayer host{0, QStringLiteral("Hôte"), {catalog.presets().first()}, true, true};
    LobbyPlayer guest{1, QStringLiteral("Invité"), {}, false, false};
    lobby.players = {host, guest};

    const LobbyInfo copy = lobbyFromJson(toJson(lobby), catalog);
    QCOMPARE(copy.name, lobby.name);
    QCOMPARE(copy.maxPlayers, 3);
    QCOMPARE(copy.stadiumId, lobby.stadiumId);
    QCOMPARE(copy.players.size(), 2);
    QCOMPARE(copy.players[0].tops.size(), 1);
    QCOMPARE(copy.players[0].tops.first().name(), host.tops.first().name());
    QVERIFY(copy.players[1].tops.isEmpty());
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

    server.setHostTops({catalog.presets().at(0)});
    QVERIFY(!server.canStart()); // Le client n'a pas encore choisi.
    client.selectTops({catalog.presets().at(2)});
    client.setReady(true);
    QTRY_VERIFY(server.canStart());
    QCOMPARE(server.lobby().players.at(1).name, QStringLiteral("Alice"));
    QCOMPARE(server.lobby().players.at(1).tops.first().name(), catalog.presets().at(2).name());

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

    server.setHostTops({catalog.presets().at(0)});
    client.selectTops({catalog.presets().at(1)});
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

void TestNet::scanSurvivesImmediateFailuresInThread()
{
    // 1200 sondes qui échouent tout de suite (Linux, macOS), dans un fil à petite pile
    // (comme sous macOS) : sans récursion, pas de débordement de pile. L'ancien code
    // débordait dès 600 sondes. Sous Windows, ces sondes attendent leur délai :
    // 1200 / 128 × 350 ms ≈ 3 s.
    QThread thread;
    thread.setStackSize(512 * 1024);
    auto *scanner = new LobbyScanner;
    scanner->moveToThread(&thread);
    QSignalSpy finished(scanner, &LobbyScanner::finished);
    thread.start();
    // Même si le test échoue : un QThread détruit en marche arrête le programme.
    const auto stop = qScopeGuard([&] {
        thread.quit();
        thread.wait();
        delete scanner;
    });
    const QList<QHostAddress> hosts(400, QHostAddress(QStringLiteral("255.255.255.255")));
    QMetaObject::invokeMethod(scanner, [scanner, hosts] { scanner->scan(hosts); });
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 20000);
}

void TestNet::hostAnswersDiscoveryQuery()
{
    // Question UDP d'un client : l'hôte répond directement à l'expéditeur.
    constexpr quint16 kQueryPort = 45991;
    LobbyServer server(PartCatalog::instance());
    QVERIFY(server.start(settings(2), 0, kQueryPort));
    QUdpSocket client;
    QVERIFY(client.bind(QHostAddress::AnyIPv4, 0));
    const QByteArray query = QJsonDocument(QJsonObject{{"type", "query"},
                                                       {"app", kApplicationTag},
                                                       {"v", kProtocolVersion}})
                                 .toJson(QJsonDocument::Compact);
    client.writeDatagram(query, QHostAddress(QHostAddress::LocalHost), kQueryPort);
    QTRY_VERIFY(client.hasPendingDatagrams());
    const QJsonObject reply = QJsonDocument::fromJson(client.receiveDatagram().data()).object();
    QCOMPARE(reply.value("name").toString(), QStringLiteral("Salon test"));
    QCOMPARE(reply.value("port").toInt(), int(server.port()));
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
    server.setHostTops({catalog.presets().at(1)});
    client->selectTops({catalog.presets().at(3)});
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

void TestNet::duelSeriesAndBotsSurviveCodec()
{
    const PartCatalog &catalog = PartCatalog::instance();
    MatchState source;
    source.spinners = std::vector<Spinner>(2);
    source.aimed = {true, true};
    source.duel.active = true;
    source.duel.first = 0;
    source.duel.second = 1;
    source.duel.point = {0.1, 0.2};
    source.duel.sides[1].cursor = -0.4;
    source.duel.sides[1].direction = -1.0;
    source.duel.sides[1].zone = 0.2;
    source.duel.sides[1].streak = 2;
    ImpactEvent end;
    end.duelLoser = 1;

    MatchState target;
    target.spinners = std::vector<Spinner>(2);
    target.aimed = {false, false};
    QList<ImpactEvent> impacts;
    QVERIFY(decodeState(encodeState(source, {end}), target, &impacts));
    QVERIFY(target.duel.active);
    QCOMPARE(target.duel.sideOf(1), 1);
    QCOMPARE(target.duel.sides[1].cursor, -0.4);
    QCOMPARE(target.duel.sides[1].direction, -1.0);
    QCOMPARE(target.duel.sides[1].streak, 2);
    QCOMPARE(impacts.first().duelLoser, 1);
    source.duel.active = false;
    QVERIFY(decodeState(encodeState(source, {}), target, nullptr));
    QVERIFY(!target.duel.active);

    MatchSetup setup;
    setup.stadium = Stadium::classic();
    setup.playerIds = {4, -1};
    setup.names = {QStringLiteral("Léa"), QStringLiteral("Bot 1")};
    setup.tops = {catalog.presets().at(0), catalog.presets().at(1)};
    setup.botLevels = {-1, static_cast<int>(Difficulty::Hard)};
    setup.match = 3;
    setup.series = MatchSeries{};
    setup.series->score = {1, 0};
    const auto parsed = parseStart(encodeStart(setup), catalog);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->playerIds, setup.playerIds);
    QCOMPARE(parsed->botLevels, setup.botLevels);
    QCOMPARE(parsed->match, 3);
    QVERIFY(parsed->series.has_value());
    QCOMPARE(parsed->series->score[0], 1);
}

namespace {
Tournament smallTournament(const QList<Participant> &humans, MatchFormat format)
{
    TournamentSettings s;
    s.format = TournamentFormat::Knockout;
    s.matchFormat = format;
    s.participants = 4;
    s.fitBots(static_cast<int>(humans.size()));
    return Tournament(s, addBots(humans, s, PartCatalog::instance().presets(), 1), 2);
}
} // namespace

void TestNet::soloTournamentPlaysAndFinishes()
{
    const QList<Top> presets = PartCatalog::instance().presets();
    TournamentDirector director(
        nullptr,
        smallTournament({{QStringLiteral("Vous"), 0, Difficulty::Normal,
                          {presets.at(0), presets.at(1), presets.at(2)}}},
                        MatchFormat::Classic),
        3);
    QSignalSpy ready(&director, &TournamentController::sessionReady);
    QSignalSpy over(&director, &TournamentController::localMatchOver);
    const int match = director.localMatch();
    QVERIFY(match >= 0);
    QVERIFY(!director.localReady());

    // Comme l'interface : la session est récupérée pendant le signal.
    std::unique_ptr<MatchSession> session;
    connect(&director, &TournamentController::sessionReady, this,
            [&] { session = director.takeSession(); });
    director.submitOrder({2, 0, 1});
    QCOMPARE(ready.count(), 1);
    QVERIFY(session);
    QVERIFY(session->state().series.has_value());
    // La première manche se joue avec la toupie choisie en premier.
    QCOMPARE(session->state().spinners[session->localPlayer()].top.name(), presets.at(2).name());

    // Le bot joue la manche jusqu'au bout (le joueur ne fait rien).
    session->submitAim(0.0, 1.0);
    for (int i = 0; i < 60 * 300 && session->state().phase != MatchPhase::Finished; ++i)
        session->update(1.0 / 60.0);
    QCOMPARE(session->state().phase, MatchPhase::Finished);
    QTRY_COMPARE(director.liveOf(match)->series.rounds, 1);
    // Manche suivante : la deuxième toupie de l'ordre choisi.
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 2, TournamentDirector::kPauseMs + 2000);
    QVERIFY(session);
    QCOMPARE(session->state().spinners[session->localPlayer()].top.name(), presets.at(0).name());

    director.forfeitLocal();
    QCOMPARE(over.count(), 1);
    QTRY_VERIFY(director.tournament().isFinished());
    QVERIFY(director.tournament().champion() != director.localParticipant());
}

void TestNet::lanTournamentStartsMatchesOnClients()
{
    const PartCatalog &catalog = PartCatalog::instance();
    LobbyServer server(catalog);
    LobbyServer::Settings lobby = settings();
    lobby.tournament = TournamentSettings{};
    lobby.tournament->participants = 4;
    QVERIFY(server.start(lobby, 0, 0));
    QCOMPARE(server.lobby().tournament->bots.size(), 2);
    LobbyClient client(catalog);
    client.connectToHost(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("Alice"));
    QTRY_VERIFY(client.isJoined());
    server.setHostTops({catalog.presets().at(0)});
    client.selectTops({catalog.presets().at(1)});
    client.setReady(true);
    QTRY_VERIFY(server.canStart());

    TournamentFollower follower(&client);
    QSignalSpy clientChanged(&follower, &TournamentController::changed);
    QSignalSpy clientReady(&follower, &TournamentController::sessionReady);
    QSignalSpy clientOver(&follower, &TournamentController::localMatchOver);

    const LobbyInfo &info = server.lobby();
    QList<Participant> humans;
    for (const LobbyPlayer &p : info.players)
        humans.append({p.name, p.id, Difficulty::Normal, p.tops});
    server.startTournament();
    TournamentDirector director(&server, smallTournament(humans, MatchFormat::Express), 9);
    QSignalSpy hostReady(&director, &TournamentController::sessionReady);

    QTRY_VERIFY(clientChanged.count() > 0);
    QCOMPARE(follower.tournament().participants().size(), 4);
    QVERIFY(follower.localParticipant() >= 0);
    QVERIFY(follower.localMatch() >= 0);

    director.submitOrder({0});
    follower.submitOrder({0});
    QTRY_COMPARE(clientReady.count(), 1);
    QCOMPARE(hostReady.count(), 1);
    std::unique_ptr<MatchSession> clientSession = follower.takeSession(); // Non pris : conservé.
    QVERIFY(clientSession && clientSession->state().series.has_value());

    follower.forfeitLocal();
    QTRY_COMPARE(clientOver.count(), 1);
    QCOMPARE(follower.localMatch(), -1);
}

void TestNet::updateIsFoundInLatestRelease()
{
    QVERIFY(isNewerVersion(QStringLiteral("v1.10.0"), QStringLiteral("1.9.2")));
    QVERIFY(!isNewerVersion(QStringLiteral("v1.0.0"), QStringLiteral("1.0.0")));
    QVERIFY(!isNewerVersion(QStringLiteral("nightly"), QStringLiteral("1.0.0")));

    const QJsonObject release{
        {"tag_name", "v1.2.0"},
        {"html_url", "https://github.com/valentinRyckaert/Top-Arena/releases/tag/v1.2.0"},
        {"assets",
         QJsonArray{QJsonObject{{"name", "ToupieArena-v1.2.0-windows-x64.zip"},
                                {"browser_download_url", "https://example.org/win.zip"}},
                    QJsonObject{{"name", "ToupieArena-v1.2.0-linux-x86_64.AppImage"},
                                {"browser_download_url", "https://example.org/linux.AppImage"}}}}};
    const auto update = parseLatestRelease(release, QStringLiteral("1.0.0"),
                                           QStringLiteral("-linux-x86_64.AppImage"));
    QVERIFY(update.has_value());
    QCOMPARE(update->version, QStringLiteral("1.2.0"));
    QCOMPARE(update->package, QUrl(QStringLiteral("https://example.org/linux.AppImage")));
    // Déjà à jour, ou aucun paquet pour cette plateforme.
    QVERIFY(!parseLatestRelease(release, QStringLiteral("1.2.0"), QStringLiteral(".AppImage")));
    QVERIFY(!parseLatestRelease(release, QStringLiteral("1.0.0"), QStringLiteral("-macos.dmg")));
}

#ifdef Q_OS_LINUX
void TestNet::appImageIsReplacedAndRelaunched()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString installed = dir.filePath(QStringLiteral("ToupieArena.AppImage"));
    QFile old(installed);
    QVERIFY(old.open(QIODevice::WriteOnly));
    old.write("ancienne version");
    old.close();
    // « Nouvelle version » : note si elle hérite de l'environnement de l'ancienne.
    const QString package = dir.filePath(QStringLiteral("nouvelle.AppImage"));
    QFile script(package);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\necho \"${APPDIR:-propre}\" > \"$(dirname \"$0\")/lancee\"\n");
    script.close();

    qputenv("APPIMAGE", installed.toLocal8Bit());
    qputenv("APPDIR", "/tmp/.mount_ancienne");
    QVERIFY(Updater::canSelfUpdate());
    QString error;
    const bool ok = Updater::installAndRestart(package, &error);
    qunsetenv("APPIMAGE");
    qunsetenv("APPDIR");
    QVERIFY2(ok, qPrintable(error));
    QVERIFY(!QFile::exists(package));
    auto launched = [&] {
        QFile marker(dir.filePath(QStringLiteral("lancee")));
        return marker.open(QIODevice::ReadOnly) ? marker.readAll().trimmed() : QByteArray();
    };
    QTRY_COMPARE(launched(), QByteArray("propre")); // Lancée, sans l'environnement de l'ancienne.
}
#endif

QTEST_GUILESS_MAIN(TestNet)
#include "tst_net.moc"
