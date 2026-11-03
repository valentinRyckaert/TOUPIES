#include "net/MatchCodec.h"

#include "net/Protocol.h"

#include "core/PartCatalog.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>

namespace toupie::net {

namespace {
/// Arrondi pour alléger les messages (précision largement suffisante).
double r(double value)
{
    return std::round(value * 10000.0) / 10000.0;
}
} // namespace

std::optional<MatchSetup> parseStart(const QJsonObject &start, const PartCatalog &catalog)
{
    MatchSetup setup;
    setup.stadium = Stadium::find(start.value("stadium").toString()).value_or(Stadium::classic());
    setup.seed = static_cast<quint32>(start.value("seed").toInteger());
    setup.rules = MatchRules::fromJson(start.value("rules").toObject());
    for (const QJsonValue &value : start.value("players").toArray()) {
        const QJsonObject o = value.toObject();
        const auto top = Top::fromJson(o.value("top").toObject(), catalog);
        if (!top)
            return std::nullopt;
        setup.playerIds.append(o.value("id").toInt(-1));
        setup.names.append(o.value("name").toString());
        setup.tops.append(*top);
    }
    if (setup.tops.size() < Battle::kMinPlayers || setup.tops.size() > Battle::kMaxPlayers)
        return std::nullopt;
    return setup;
}

QJsonObject encodeState(const MatchState &state, const QList<ImpactEvent> &impacts)
{
    QJsonArray spinners;
    for (const Spinner &s : state.spinners) {
        spinners.append(QJsonArray{r(s.position.x), r(s.position.y), r(s.velocity.x),
                                   r(s.velocity.y), r(s.spin), r(s.angle), r(s.dashCooldown),
                                   r(s.boostTimer), r(s.dashTimer), static_cast<int>(s.status),
                                   s.specialUsed ? 1 : 0, r(s.slowTimer), r(s.disarmTimer)});
    }
    QJsonArray projectiles;
    for (const Projectile &p : state.projectiles) {
        projectiles.append(QJsonArray{p.id, p.move, p.owner, p.ring ? 1 : 0, r(p.origin.x),
                                      r(p.origin.y), r(p.position.x), r(p.position.y),
                                      r(p.direction.x), r(p.direction.y), r(p.radius),
                                      r(p.traveled), r(p.speed), r(p.range), r(p.width),
                                      static_cast<qint64>(p.color)});
    }
    QJsonArray aimed;
    for (bool a : state.aimed)
        aimed.append(a);
    QJsonArray ranking;
    for (int i : state.ranking)
        ranking.append(i);
    QJsonArray hits;
    for (const ImpactEvent &impact : impacts)
        hits.append(QJsonArray{r(impact.point.x), r(impact.point.y), r(impact.intensity),
                               impact.special ? static_cast<qint64>(impact.color | 0x1000000u) : 0});

    return {
        {"type", Msg::State},
        {"ph", static_cast<int>(state.phase)},
        {"cd", r(state.countdown)},
        {"el", r(state.elapsed)},
        {"aim", aimed},
        {"w", state.winner},
        {"why", static_cast<int>(state.endReason)},
        {"rank", ranking},
        {"s", spinners},
        {"p", projectiles},
        {"imp", hits},
    };
}

bool decodeState(const QJsonObject &message, MatchState &state, QList<ImpactEvent> *impacts)
{
    const QJsonArray spinners = message.value("s").toArray();
    if (spinners.size() != state.playerCount())
        return false;

    for (int i = 0; i < spinners.size(); ++i) {
        const QJsonArray a = spinners.at(i).toArray();
        if (a.size() < 13)
            return false;
        Spinner &s = state.spinners[i];
        s.position = {a.at(0).toDouble(), a.at(1).toDouble()};
        s.velocity = {a.at(2).toDouble(), a.at(3).toDouble()};
        s.spin = a.at(4).toDouble();
        s.angle = a.at(5).toDouble();
        s.dashCooldown = a.at(6).toDouble();
        s.boostTimer = a.at(7).toDouble();
        s.dashTimer = a.at(8).toDouble();
        s.status = static_cast<Spinner::Status>(
            std::clamp(a.at(9).toInt(), 0, static_cast<int>(Spinner::Status::Forfeit)));
        s.specialUsed = a.at(10).toInt() != 0;
        s.slowTimer = a.at(11).toDouble();
        s.disarmTimer = a.at(12).toDouble();
    }

    state.projectiles.clear();
    for (const QJsonValue &v : message.value("p").toArray()) {
        const QJsonArray a = v.toArray();
        if (a.size() < 16)
            continue;
        Projectile p;
        p.id = a.at(0).toInt();
        p.move = a.at(1).toString();
        p.owner = a.at(2).toInt();
        p.ring = a.at(3).toInt() != 0;
        p.origin = {a.at(4).toDouble(), a.at(5).toDouble()};
        p.position = {a.at(6).toDouble(), a.at(7).toDouble()};
        p.direction = {a.at(8).toDouble(), a.at(9).toDouble()};
        p.radius = a.at(10).toDouble();
        p.traveled = a.at(11).toDouble();
        p.speed = a.at(12).toDouble();
        p.range = a.at(13).toDouble();
        p.width = a.at(14).toDouble();
        p.color = static_cast<unsigned>(a.at(15).toInteger());
        state.projectiles.push_back(p);
    }

    const QJsonArray aimed = message.value("aim").toArray();
    for (int i = 0; i < state.aimed.size() && i < aimed.size(); ++i)
        state.aimed[i] = aimed.at(i).toBool();

    state.phase = static_cast<MatchPhase>(
        std::clamp(message.value("ph").toInt(), 0, static_cast<int>(MatchPhase::Finished)));
    state.countdown = message.value("cd").toDouble();
    state.elapsed = message.value("el").toDouble();
    state.winner = message.value("w").toInt(-1);
    state.endReason = static_cast<Battle::EndReason>(
        std::clamp(message.value("why").toInt(), 0, static_cast<int>(Battle::EndReason::Forfeit)));
    state.ranking.clear();
    for (const QJsonValue &v : message.value("rank").toArray())
        state.ranking.append(v.toInt());

    if (impacts) {
        for (const QJsonValue &v : message.value("imp").toArray()) {
            const QJsonArray a = v.toArray();
            ImpactEvent impact;
            impact.point = {a.at(0).toDouble(), a.at(1).toDouble()};
            impact.intensity = a.at(2).toDouble();
            impact.special = a.size() > 3 && a.at(3).toInteger() != 0;
            impact.color = static_cast<unsigned>(a.at(3).toInteger()) & 0xFFFFFFu;
            impacts->append(impact);
        }
    }
    return true;
}

} // namespace toupie::net
