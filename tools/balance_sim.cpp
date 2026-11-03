// Simulateur d'équilibrage : fait s'affronter des bots et affiche les taux de
// victoire, d'une part entre les toupies pré-assemblées, d'autre part entre
// types, sur des toupies tirées au hasard parmi toutes les combinaisons de
// pièces possibles.
//
// Usage : toupie_balance_sim [options] [paramètre=valeur ...]
//   -n <nombre>       combats par confrontation de presets (défaut 200)
//   -r <nombre>       combats aléatoires pour la matrice des types (défaut 4000)
//   -d <niveau>       facile | normal | difficile (défaut normal)
//   -s <stadium|all>  stadium (identifiant, défaut classique) ou tous les stadiums
//   -p <2..4>         nombre de toupies par combat (mêlée générale au-delà de 2)
//   paramètre=valeur  surcharge d'un champ de BattleConfig (ex. dashCost=8)

#include "core/PartCatalog.h"
#include "game/Battle.h"
#include "game/BotController.h"
#include "game/MatchRules.h"
#include "game/SpecialMove.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <random>
#include <vector>

using namespace toupie;

namespace {

struct Outcome
{
    int winner = -1; ///< Indice dans la liste fournie, -1 = nul.
    Battle::EndReason reason = Battle::EndReason::None;
    double duration = 0.0;
};

Outcome simulate(const QList<Top> &tops, const Stadium &stadium, const BattleConfig &config,
                 Difficulty difficulty, std::uint32_t seed)
{
    // On fait tourner les places pour neutraliser l'avantage de position.
    const int n = static_cast<int>(tops.size());
    const int shift = static_cast<int>(seed % static_cast<std::uint32_t>(n));
    QList<Top> seated;
    for (int i = 0; i < n; ++i)
        seated.append(tops.at((i + shift) % n));

    const double dt = 1.0 / 60.0;
    Battle battle(seated, stadium, config, seed);
    std::vector<BotController> bots;
    QList<double> angles;
    QList<double> powers;
    for (int i = 0; i < n; ++i) {
        bots.emplace_back(i, difficulty, seed * 7 + static_cast<std::uint32_t>(i) + 1);
        angles.append(bots.back().chooseLaunchAngle(battle));
        powers.append(bots.back().chooseLaunchPower(battle));
    }
    battle.launch(angles, powers);
    while (!battle.isFinished()) {
        for (BotController &bot : bots)
            bot.update(battle, dt);
        battle.advance(dt);
    }

    Outcome outcome;
    outcome.reason = battle.endReason();
    outcome.duration = battle.elapsed();
    if (battle.winner() >= 0)
        outcome.winner = (battle.winner() + shift) % n;
    return outcome;
}

Outcome simulate(const Top &a, const Top &b, const Stadium &stadium, const BattleConfig &config,
                 Difficulty difficulty, std::uint32_t seed)
{
    return simulate(QList<Top>{a, b}, stadium, config, difficulty, seed);
}

constexpr int kTypes = 4;
const TopType kTypeOrder[kTypes] = {TopType::Attack, TopType::Defense, TopType::Endurance,
                                    TopType::Balance};

struct TypeMatrix
{
    std::array<std::array<double, kTypes>, kTypes> score{};
    std::array<std::array<int, kTypes>, kTypes> games{};
    int fights = 0;
    int ringOuts = 0;
    double duration = 0.0;

    double rate(int i, int j) const { return games[i][j] ? 100.0 * score[i][j] / games[i][j] : 0.0; }
    double global(int i) const
    {
        double total = 0.0;
        int count = 0;
        for (int j = 0; j < kTypes; ++j) {
            total += score[i][j];
            count += games[i][j];
        }
        return count ? 100.0 * total / count : 0.0;
    }
};

/// Taux de victoire de chaque coup spécial (duels entre boulons différents).
std::map<QString, double> g_specialScore;
std::map<QString, int> g_specialGames;

TypeMatrix runTypeMatrix(const QList<Top> &pool, const Stadium &stadium,
                         const BattleConfig &config, Difficulty difficulty, int fights)
{
    TypeMatrix m;
    std::mt19937 rng(42);
    std::uniform_int_distribution<qsizetype> pick(0, pool.size() - 1);
    for (int k = 0; k < fights; ++k) {
        const Top &a = pool.at(pick(rng));
        const Top &b = pool.at(pick(rng));
        const int ta = static_cast<int>(a.type());
        const int tb = static_cast<int>(b.type());
        const Outcome o = simulate(a, b, stadium, config, difficulty, static_cast<std::uint32_t>(k));
        const double sa = o.winner == 0 ? 1.0 : (o.winner == -1 ? 0.5 : 0.0);
        if (a.special() != b.special()) {
            g_specialScore[a.special()] += sa;
            g_specialScore[b.special()] += 1.0 - sa;
            ++g_specialGames[a.special()];
            ++g_specialGames[b.special()];
        }
        m.score[ta][tb] += sa;
        m.score[tb][ta] += 1.0 - sa;
        ++m.games[ta][tb];
        ++m.games[tb][ta];
        m.ringOuts += o.reason == Battle::EndReason::RingOut;
        m.duration += o.duration;
    }
    m.fights = fights;
    return m;
}

Difficulty parseDifficulty(const QString &text)
{
    if (text.startsWith(QLatin1String("f")) || text.startsWith(QLatin1String("e")))
        return Difficulty::Easy;
    if (text.startsWith(QLatin1String("d")) || text.startsWith(QLatin1String("h")))
        return Difficulty::Hard;
    return Difficulty::Normal;
}

std::map<QString, double BattleConfig::*> configFields()
{
    return {
        {"launchSpeed", &BattleConfig::launchSpeed},
        {"rimEscapeSpeed", &BattleConfig::rimEscapeSpeed},
        {"rimRestitution", &BattleConfig::rimRestitution},
        {"slopeStrength", &BattleConfig::slopeStrength},
        {"friction", &BattleConfig::friction},
        {"orbitBase", &BattleConfig::orbitBase},
        {"orbitPerAttack", &BattleConfig::orbitPerAttack},
        {"passiveBase", &BattleConfig::passiveBase},
        {"passivePerEndurance", &BattleConfig::passivePerEndurance},
        {"speedDrain", &BattleConfig::speedDrain},
        {"damageBase", &BattleConfig::damageBase},
        {"damagePerDefense", &BattleConfig::damagePerDefense},
        {"minImpact", &BattleConfig::minImpact},
        {"impactRef", &BattleConfig::impactRef},
        {"maxImpact", &BattleConfig::maxImpact},
        {"knockbackBase", &BattleConfig::knockbackBase},
        {"knockbackPerAttack", &BattleConfig::knockbackPerAttack},
        {"knockbackPerAttackerDefense", &BattleConfig::knockbackPerAttackerDefense},
        {"knockbackApproach", &BattleConfig::knockbackApproach},
        {"knockbackPerDefense", &BattleConfig::knockbackPerDefense},
        {"spinDeflection", &BattleConfig::spinDeflection},
        {"sameSpinKnockback", &BattleConfig::sameSpinKnockback},
        {"oppositeSpinDamage", &BattleConfig::oppositeSpinDamage},
        {"contactCooldown", &BattleConfig::contactCooldown},
        {"balanceStability", &BattleConfig::balanceStability},
        {"knockoutReward", &BattleConfig::knockoutReward},
        {"knockoutWindow", &BattleConfig::knockoutWindow},
        {"dashCost", &BattleConfig::dashCost},
        {"dashSpeed", &BattleConfig::dashSpeed},
        {"dashDuration", &BattleConfig::dashDuration},
        {"dashCooldown", &BattleConfig::dashCooldown},
        {"dashAttackMultiplier", &BattleConfig::dashAttackMultiplier},
        {"dashBoostDuration", &BattleConfig::dashBoostDuration},
        {"centerPull", &BattleConfig::centerPull},
        {"steerAcceleration", &BattleConfig::steerAcceleration},
        {"specialChargeRate", &BattleConfig::specialChargeRate},
        {"specialHitCharge", &BattleConfig::specialHitCharge},
        {"specialDamageScale", &BattleConfig::specialDamageScale},
        {"maxDuration", &BattleConfig::maxDuration},
    };
}

QList<Top> allCombinations(const PartCatalog &catalog)
{
    QList<Top> tops;
    for (const Part &w : catalog.parts(PartKind::Wheel))
        for (const Part &m : catalog.parts(PartKind::Middle))
            for (const Part &t : catalog.parts(PartKind::Tip))
                for (const Part &b : catalog.parts(PartKind::Bolt))
                    tops.append(Top(w.name + '/' + m.name + '/' + t.name, w, m, t, b));
    return tops;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    int presetFights = 200;
    int randomFights = 4000;
    int players = 2;
    QList<Stadium> stadiums{Stadium::classic()};
    Difficulty difficulty = Difficulty::Normal;
    BattleConfig config;
    const auto fields = configFields();

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (arg == QLatin1String("-n") && i + 1 < args.size()) {
            presetFights = qMax(0, args.at(++i).toInt());
        } else if (arg == QLatin1String("-r") && i + 1 < args.size()) {
            randomFights = qMax(0, args.at(++i).toInt());
        } else if (arg == QLatin1String("-d") && i + 1 < args.size()) {
            difficulty = parseDifficulty(args.at(++i));
        } else if (arg == QLatin1String("-p") && i + 1 < args.size()) {
            players = std::clamp(args.at(++i).toInt(), Battle::kMinPlayers, Battle::kMaxPlayers);
        } else if (arg == QLatin1String("-s") && i + 1 < args.size()) {
            const QString id = args.at(++i);
            if (id == QLatin1String("all")) {
                stadiums = Stadium::all();
            } else if (const auto stadium = Stadium::find(id)) {
                stadiums = {*stadium};
            } else {
                err << "Stadium inconnu : " << id << "\n";
                return 1;
            }
        } else if (arg.contains('=')) {
            const QString key = arg.section('=', 0, 0);
            const auto it = fields.find(key);
            if (it == fields.end()) {
                err << "Paramètre inconnu : " << key << "\n";
                return 1;
            }
            bool ok = false;
            const double value = arg.section('=', 1).toDouble(&ok);
            if (!ok) {
                err << "Valeur invalide pour " << key << " : " << arg.section('=', 1) << "\n";
                return 1;
            }
            config.*(it->second) = value;
        } else {
            err << "Argument invalide : " << arg << "\n";
            return 1;
        }
    }

    out << "Difficulté des bots : " << toString(difficulty) << "\n";

    // --- Presets ------------------------------------------------------------
    const QList<Top> presets = PartCatalog::instance().presets();
    if (presetFights > 0 && players == 2 && stadiums.size() == 1) {
        out << "\nPresets (" << presetFights << " combats par confrontation)\n";
        for (int i = 0; i < presets.size(); ++i) {
            for (int j = i + 1; j < presets.size(); ++j) {
                int wins = 0, draws = 0, ringOuts = 0;
                double duration = 0.0;
                for (int k = 0; k < presetFights; ++k) {
                    const Outcome o = simulate(presets.at(i), presets.at(j), stadiums.first(),
                                               config, difficulty,
                                               1000u + static_cast<std::uint32_t>(k));
                    wins += o.winner == 0;
                    draws += o.winner == -1;
                    ringOuts += o.reason == Battle::EndReason::RingOut;
                    duration += o.duration;
                }
                const double rate = 100.0 * (wins + 0.5 * draws) / presetFights;
                out << qSetFieldWidth(16) << Qt::left << presets.at(i).name()
                    << qSetFieldWidth(0) << " vs " << qSetFieldWidth(16)
                    << presets.at(j).name() << qSetFieldWidth(0)
                    << QString::asprintf(" %5.1f%%  (éjections %4.1f%%, durée moy. %4.1fs)\n",
                                         rate, 100.0 * ringOuts / presetFights,
                                         duration / presetFights);
            }
        }
    }

    const QList<Top> pool = allCombinations(PartCatalog::instance());

    // --- Matrice des types --------------------------------------------------
    if (randomFights > 0 && players == 2 && stadiums.size() == 1) {
        const TypeMatrix m = runTypeMatrix(pool, stadiums.first(), config, difficulty, randomFights);
        out << "\nTypes (" << randomFights << " combats entre toupies aléatoires, stadium "
            << stadiums.first().name << ")\n"
            << "  ligne = taux de victoire contre la colonne\n"
            << qSetFieldWidth(12) << Qt::left << "";
        for (TopType t : kTypeOrder)
            out << qSetFieldWidth(11) << Qt::right << toString(t);
        out << qSetFieldWidth(11) << "Global" << qSetFieldWidth(0) << "\n";

        double worst = 50.0;
        for (int i = 0; i < kTypes; ++i) {
            out << qSetFieldWidth(12) << Qt::left << toString(kTypeOrder[i]) << qSetFieldWidth(0);
            for (int j = 0; j < kTypes; ++j)
                out << QString::asprintf("%10.1f%%", m.rate(i, j));
            worst = qMin(worst, m.global(i));
            out << QString::asprintf("%10.1f%%\n", m.global(i));
        }
        out << QString::asprintf("\nÉjections : %.1f%%, durée moyenne : %.1fs, pire type : %.1f%%\n",
                                 100.0 * m.ringOuts / m.fights, m.duration / m.fights, worst);
        out << "\nCoups spéciaux (duels entre boulons différents)\n";
        for (const auto &[id, score] : g_specialScore) {
            const auto move = SpecialMove::find(id);
            out << "  " << qSetFieldWidth(18) << Qt::left << (move ? move->name : id)
                << qSetFieldWidth(0)
                << QString::asprintf("%5.1f%%\n", 100.0 * score / g_specialGames[id]);
        }
    } else if (randomFights > 0 && players == 2) {
        // Résumé par stadium : taux de victoire global de chaque type.
        out << "\nTaux de victoire global par type et par stadium (" << randomFights
            << " combats chacun)\n"
            << qSetFieldWidth(12) << Qt::left << "Stadium";
        for (TopType t : kTypeOrder)
            out << qSetFieldWidth(11) << Qt::right << toString(t);
        out << qSetFieldWidth(12) << "Pire duel" << qSetFieldWidth(11) << "Éjections"
            << qSetFieldWidth(9) << "Durée" << qSetFieldWidth(0) << "\n";
        for (const Stadium &stadium : stadiums) {
            const TypeMatrix m = runTypeMatrix(pool, stadium, config, difficulty, randomFights);
            out << qSetFieldWidth(12) << Qt::left << stadium.name << qSetFieldWidth(0);
            double worstDuel = 50.0;
            for (int i = 0; i < kTypes; ++i) {
                out << QString::asprintf("%10.1f%%", m.global(i));
                for (int j = 0; j < kTypes; ++j)
                    worstDuel = qMin(worstDuel, m.rate(i, j));
            }
            out << QString::asprintf("%11.1f%%%10.1f%%%8.1fs\n", worstDuel,
                                     100.0 * m.ringOuts / m.fights, m.duration / m.fights);
        }
    } else if (randomFights > 0) {
        // Mêlée générale : victoires par type rapportées à la part attendue (1/N).
        out << "\nMêlée à " << players << " (" << randomFights
            << " combats) — indice 100 % = victoires conformes au hasard\n"
            << qSetFieldWidth(12) << Qt::left << "Stadium";
        for (TopType t : kTypeOrder)
            out << qSetFieldWidth(11) << Qt::right << toString(t);
        out << qSetFieldWidth(9) << "Durée" << qSetFieldWidth(0) << "\n";
        for (const Stadium &stadium : stadiums) {
            std::mt19937 rng(7);
            std::uniform_int_distribution<qsizetype> pick(0, pool.size() - 1);
            std::array<double, kTypes> wins{};
            std::array<int, kTypes> appearances{};
            double duration = 0.0;
            for (int k = 0; k < randomFights; ++k) {
                QList<Top> tops;
                for (int p = 0; p < players; ++p)
                    tops.append(pool.at(pick(rng)));
                const Outcome o = simulate(tops, stadium, config, difficulty,
                                           static_cast<std::uint32_t>(k));
                for (const Top &t : tops)
                    ++appearances[static_cast<int>(t.type())];
                if (o.winner >= 0)
                    wins[static_cast<int>(tops.at(o.winner).type())] += 1.0;
                duration += o.duration;
            }
            out << qSetFieldWidth(12) << Qt::left << stadium.name << qSetFieldWidth(0);
            for (int i = 0; i < kTypes; ++i) {
                const double index = appearances[i] ? 100.0 * wins[i] * players / appearances[i]
                                                    : 0.0;
                out << QString::asprintf("%10.1f%%", index);
            }
            out << QString::asprintf("%8.1fs\n", duration / randomFights);
        }
    }
    return 0;
}
