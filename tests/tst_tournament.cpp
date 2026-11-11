#include "core/PartCatalog.h"
#include "game/Tournament.h"

#include <QSet>
#include <QStandardPaths>
#include <QTest>

using namespace toupie;

namespace {

QList<Participant> makeParticipants(int count, int tops = 1)
{
    const QList<Top> presets = PartCatalog::instance().presets();
    QList<Participant> list;
    for (int i = 0; i < count; ++i) {
        Participant p;
        p.name = QStringLiteral("P%1").arg(i);
        p.playerId = i == 0 ? 0 : -1;
        for (int k = 0; k < tops; ++k)
            p.tops.append(presets.at((i + k) % presets.size()));
        list.append(p);
    }
    return list;
}

TournamentSettings settings(TournamentFormat format, MatchFormat match, int participants)
{
    TournamentSettings s;
    s.format = format;
    s.matchFormat = match;
    s.participants = participants;
    s.fitBots(1);
    return s;
}

/// Score fictif : le participant d'indice le plus petit gagne 2-1.
MatchSeries fakeResult(const TournamentMatch &m)
{
    MatchSeries s;
    s.score = m.a < m.b ? std::array<int, 2>{2, 1} : std::array<int, 2>{1, 2};
    return s;
}

void playRound(Tournament &t)
{
    const QList<TournamentMatch> round = t.rounds().at(t.currentRound());
    for (int i = 0; i < round.size(); ++i) {
        if (!round.at(i).played)
            t.record(i, fakeResult(round.at(i)));
    }
}

} // namespace

class TestTournament : public QObject
{
    Q_OBJECT

private slots:
    void expressSeriesFirstToTwoRounds();
    void classicSeriesCountsFinishes();
    void classicSeriesCyclesTopOrder();
    void leagueEveryoneMeetsOnce_data();
    void leagueEveryoneMeetsOnce();
    void leagueStandingsAddBothScores();
    void knockoutBracketWithByes();
    void jsonRoundTrip();
    void savedTournamentRoundTrip();
    void botMatchHasWinner();
};

void TestTournament::expressSeriesFirstToTwoRounds()
{
    MatchSeries s;
    s.recordRound(0, Battle::EndReason::RingOut);
    QCOMPARE(s.score[0], 1); // Express : une manche = un point, quelle que soit la fin.
    s.recordRound(-1, Battle::EndReason::SpinOut);
    QCOMPARE(s.rounds, 2);
    QVERIFY(!s.isOver());
    s.recordRound(1, Battle::EndReason::SpinOut);
    s.recordRound(0, Battle::EndReason::SpinOut);
    QVERIFY(s.isOver());
    QCOMPARE(s.winner(), 0);
    QCOMPARE(s.score[1], 1);
}

void TestTournament::classicSeriesCountsFinishes()
{
    MatchSeries s;
    s.format = MatchFormat::Classic;
    s.recordRound(1, Battle::EndReason::RingOut);
    QCOMPARE(s.score[1], 2);
    s.recordRound(0, Battle::EndReason::SpinOut);
    QCOMPARE(s.score[0], 1);
    s.recordRound(1, Battle::EndReason::SpinOut);
    QVERIFY(!s.isOver());
    s.recordRound(1, Battle::EndReason::SpinOut);
    QCOMPARE(s.winner(), 1);

    MatchSeries forfeit;
    forfeit.format = MatchFormat::Classic;
    forfeit.forfeit(0);
    QCOMPARE(forfeit.winner(), 1);
}

void TestTournament::classicSeriesCyclesTopOrder()
{
    MatchSeries s;
    s.format = MatchFormat::Classic;
    s.order[0] = {2, 0, 1};
    QList<int> played;
    for (int r = 0; r < 4; ++r) {
        played.append(s.topFor(0));
        s.recordRound(-1, Battle::EndReason::SpinOut);
    }
    QCOMPARE(played, (QList<int>{2, 0, 1, 2}));
}

void TestTournament::leagueEveryoneMeetsOnce_data()
{
    QTest::addColumn<int>("count");
    QTest::newRow("4") << 4;
    QTest::newRow("5 (exempt)") << 5;
    QTest::newRow("32") << 32;
}

void TestTournament::leagueEveryoneMeetsOnce()
{
    QFETCH(int, count);
    Tournament t(settings(TournamentFormat::League, MatchFormat::Express, count),
                 makeParticipants(count), 7);
    QCOMPARE(t.rounds().size(), t.roundCount());
    QSet<QPair<int, int>> pairs;
    for (const QList<TournamentMatch> &round : t.rounds()) {
        QSet<int> seen;
        for (const TournamentMatch &m : round) {
            QVERIFY(!seen.contains(m.a));
            seen.insert(m.a);
            if (m.isBye())
                continue;
            QVERIFY(!seen.contains(m.b));
            seen.insert(m.b);
            pairs.insert({qMin(m.a, m.b), qMax(m.a, m.b)});
        }
        QCOMPARE(seen.size(), count);
    }
    QCOMPARE(pairs.size(), count * (count - 1) / 2);

    while (!t.isFinished()) {
        playRound(t);
        t.nextRound();
    }
    QCOMPARE(t.champion(), 0); // Le plus petit indice gagne tous ses matchs.
}

void TestTournament::leagueStandingsAddBothScores()
{
    Tournament t(settings(TournamentFormat::League, MatchFormat::Express, 4), makeParticipants(4),
                 1);
    const TournamentMatch m = t.rounds().first().first();
    MatchSeries s;
    s.score = {2, 1};
    t.record(0, s);
    for (const Standing &st : t.standings()) {
        if (st.participant == m.a) {
            QCOMPARE(st.points, 2);
            QCOMPARE(st.won, 1);
        } else if (st.participant == m.b) {
            QCOMPARE(st.points, 1); // Le perdant marque aussi ses manches gagnées.
            QCOMPARE(st.lost, 1);
        } else {
            QCOMPARE(st.played, 0);
        }
    }
}

void TestTournament::knockoutBracketWithByes()
{
    Tournament t(settings(TournamentFormat::Knockout, MatchFormat::Express, 6), makeParticipants(6),
                 3);
    QCOMPARE(t.roundCount(), 3);
    const QList<TournamentMatch> first = t.rounds().first();
    QCOMPARE(first.size(), 4);
    int byes = 0;
    for (const TournamentMatch &m : first) {
        QVERIFY(m.a >= 0); // Jamais deux exemptions face à face.
        byes += m.isBye();
        QCOMPARE(m.played, m.isBye());
    }
    QCOMPARE(byes, 2);
    QCOMPARE(t.roundName(2), QStringLiteral("Finale"));

    while (!t.isFinished()) {
        QVERIFY(t.currentRound() < 3);
        playRound(t);
        t.nextRound();
    }
    QCOMPARE(t.rounds().last().size(), 1);
    QCOMPARE(t.champion(), 0);
}

void TestTournament::jsonRoundTrip()
{
    Tournament t(settings(TournamentFormat::Knockout, MatchFormat::Classic, 8),
                 makeParticipants(8, 3), 5);
    playRound(t);
    t.nextRound();
    const auto copy = Tournament::fromJson(t.toJson(), PartCatalog::instance());
    QVERIFY(copy.has_value());
    QCOMPARE(copy->toJson(), t.toJson());
    QCOMPARE(copy->participants().at(3).tops.size(), 3);
    QCOMPARE(copy->currentRound(), 1);
}

void TestTournament::savedTournamentRoundTrip()
{
    QStandardPaths::setTestModeEnabled(true); // Jamais le tournoi du joueur.
    SavedTournament::remove();
    QVERIFY(!SavedTournament::exists());
    QVERIFY(!SavedTournament::load(PartCatalog::instance()));
    Tournament t(settings(TournamentFormat::League, MatchFormat::Express, 6), makeParticipants(6), 9);
    playRound(t);
    t.nextRound();
    QVERIFY(SavedTournament::save(t));
    QVERIFY(SavedTournament::exists());
    const auto loaded = SavedTournament::load(PartCatalog::instance());
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->toJson(), t.toJson());
    SavedTournament::remove();
    QVERIFY(!SavedTournament::exists());
}

void TestTournament::botMatchHasWinner()
{
    const QList<Participant> p = makeParticipants(2, 3);
    const MatchSeries s = playBotMatch(p.at(0), p.at(1), MatchFormat::Classic, Stadium::classic(),
                                       {}, 11);
    QVERIFY(s.isOver());
    QVERIFY(s.score[s.winner()] >= 4);
    QVERIFY(s.rounds >= 2);
}

QTEST_GUILESS_MAIN(TestTournament)
#include "tst_tournament.moc"
