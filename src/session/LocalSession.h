#pragma once

#include "game/BotController.h"
#include "session/MatchSession.h"

#include <memory>
#include <vector>

namespace toupie {

/// Combat en solo : le joueur (indice 0) contre un ou plusieurs bots.
class LocalSession : public MatchSession
{
    Q_OBJECT

public:
    /// @param tops  toupie du joueur puis celles des bots (2 à 4 au total)
    LocalSession(const QList<Top> &tops, const QStringList &names, const Stadium &stadium,
                 Difficulty difficulty, const MatchRules &rules = {}, QObject *parent = nullptr);
    ~LocalSession() override;

    void restart();

    void update(double dt) override;
    void submitAim(double angle, double power) override;
    void requestDash() override;
    void requestSpecial() override;
    void setSteering(Vec2 direction) override;

private:
    QList<Top> m_tops;
    QStringList m_names;
    Stadium m_stadium;
    Difficulty m_difficulty;
    MatchRules m_rules;

    std::unique_ptr<Battle> m_battle;
    std::vector<BotController> m_bots; ///< Bot i pilote la toupie i + 1.
    QList<double> m_angles;
    QList<double> m_powers;
};

} // namespace toupie
