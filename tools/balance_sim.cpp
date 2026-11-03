// Simulateur d'équilibrage : fait s'affronter des bots et affiche les taux de
// victoire, d'une part entre les toupies pré-assemblées, d'autre part entre
// types, sur des toupies tirées au hasard parmi toutes les combinaisons de
// pièces possibles.
//
// Usage : toupie_balance_sim [options] [paramètre=valeur ...]
//   -n <nombre>       combats par confrontation de presets (défaut 200)
//   -r <nombre>       combats aléatoires pour la matrice des types (défaut 4000)
//   -d <niveau>       facile | normal | difficile (défaut normal)
//   paramètre=valeur  surcharge d'un champ de BattleConfig (ex. dashCost=8)

#include "core/PartCatalog.h"
#include "game/Battle.h"
#include "game/BotController.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include <array>
#include <cstdint>
#include <map>
#include <random>

using namespace toupie;

namespace {

struct Outcome
{
    int winner = -1; ///< 0 = première toupie, 1 = seconde, -1 = nul.
    Battle::EndReason reason = Battle::EndReason::None;
    double duration = 0.0;
};

Outcome simulate(const Top &a, const Top &b, const BattleConfig &config, Difficulty difficulty,
                 std::uint32_t seed)
{
    // On alterne les côtés pour neutraliser l'avantage de position.
    const bool swapped = (seed % 2) == 1;
    const double dt = 1.0 / 60.0;
    Battle battle(swapped ? b : a, swapped ? a : b, config, seed);
    BotController bot0(0, difficulty, seed * 3 + 1);
    BotController bot1(1, difficulty, seed * 3 + 2);
    battle.launch(bot0.chooseLaunchAngle(battle), bot1.chooseLaunchAngle(battle));
    while (!battle.isFinished()) {
        bot0.update(battle, dt);
        bot1.update(battle, dt);
        battle.advance(dt);
    }

    Outcome outcome;
    outcome.reason = battle.endReason();
    outcome.duration = battle.elapsed();
    if (battle.winner() >= 0)
        outcome.winner = swapped ? 1 - battle.winner() : battle.winner();
    return outcome;
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
        {"dashCost", &BattleConfig::dashCost},
        {"dashSpeed", &BattleConfig::dashSpeed},
        {"dashDuration", &BattleConfig::dashDuration},
        {"dashCooldown", &BattleConfig::dashCooldown},
        {"dashAttackMultiplier", &BattleConfig::dashAttackMultiplier},
        {"dashBoostDuration", &BattleConfig::dashBoostDuration},
    };
}

QList<Top> allCombinations(const PartCatalog &catalog)
{
    QList<Top> tops;
    for (const Part &w : catalog.parts(PartKind::Wheel))
        for (const Part &m : catalog.parts(PartKind::Middle))
            for (const Part &t : catalog.parts(PartKind::Tip))
                tops.append(Top(w.name + '/' + m.name + '/' + t.name, w, m, t));
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
    if (presetFights > 0) {
        out << "\nPresets (" << presetFights << " combats par confrontation)\n";
        for (int i = 0; i < presets.size(); ++i) {
            for (int j = i + 1; j < presets.size(); ++j) {
                int wins = 0, draws = 0, ringOuts = 0;
                double duration = 0.0;
                for (int k = 0; k < presetFights; ++k) {
                    const Outcome o = simulate(presets.at(i), presets.at(j), config, difficulty,
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

    // --- Matrice des types --------------------------------------------------
    if (randomFights > 0) {
        const QList<Top> pool = allCombinations(PartCatalog::instance());
        std::mt19937 rng(42);
        std::uniform_int_distribution<qsizetype> pick(0, pool.size() - 1);

        constexpr int kTypes = 4;
        std::array<std::array<double, kTypes>, kTypes> score{};
        std::array<std::array<int, kTypes>, kTypes> games{};
        int ringOuts = 0;
        double duration = 0.0;

        for (int k = 0; k < randomFights; ++k) {
            const Top &a = pool.at(pick(rng));
            const Top &b = pool.at(pick(rng));
            const int ta = static_cast<int>(a.type());
            const int tb = static_cast<int>(b.type());
            const Outcome o = simulate(a, b, config, difficulty, static_cast<std::uint32_t>(k));
            const double sa = o.winner == 0 ? 1.0 : (o.winner == -1 ? 0.5 : 0.0);
            score[ta][tb] += sa;
            score[tb][ta] += 1.0 - sa;
            ++games[ta][tb];
            ++games[tb][ta];
            ringOuts += o.reason == Battle::EndReason::RingOut;
            duration += o.duration;
        }

        const TopType types[kTypes] = {TopType::Attack, TopType::Defense, TopType::Endurance,
                                       TopType::Balance};
        out << "\nTypes (" << randomFights << " combats entre toupies aléatoires)\n"
            << "  ligne = taux de victoire contre la colonne\n"
            << qSetFieldWidth(12) << Qt::left << "";
        for (TopType t : types)
            out << qSetFieldWidth(11) << Qt::right << toString(t);
        out << qSetFieldWidth(11) << "Global" << qSetFieldWidth(0) << "\n";

        double worst = 50.0;
        for (int i = 0; i < kTypes; ++i) {
            out << qSetFieldWidth(12) << Qt::left << toString(types[i]) << qSetFieldWidth(0);
            double total = 0.0;
            int count = 0;
            for (int j = 0; j < kTypes; ++j) {
                const double rate = games[i][j] ? 100.0 * score[i][j] / games[i][j] : 0.0;
                out << QString::asprintf("%10.1f%%", rate);
                total += score[i][j];
                count += games[i][j];
            }
            const double global = count ? 100.0 * total / count : 0.0;
            worst = qMin(worst, global);
            out << QString::asprintf("%10.1f%%\n", global);
        }
        out << QString::asprintf("\nÉjections : %.1f%%, durée moyenne : %.1fs, pire type : %.1f%%\n",
                                 100.0 * ringOuts / randomFights, duration / randomFights, worst);
    }
    return 0;
}
