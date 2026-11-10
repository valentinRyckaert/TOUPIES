#pragma once

#include "core/Top.h"
#include "game/BotController.h"
#include "game/MatchRules.h"

#include <QList>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QLabel;

namespace toupie {

class RulesWidget;
class StadiumWidget;
class StatsWidget;
class TopChip;
class TopRepository;

/// Préparation d'un combat solo : sa toupie, 1 à 3 bots (toupie choisie ou au
/// hasard), leur difficulté, le stadium et les règles.
class SetupPage : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kMaxBots = 3;

    explicit SetupPage(TopRepository &repository, QWidget *parent = nullptr);

    /// Recharge la liste des toupies disponibles en conservant la sélection.
    void refresh();

    int botCount() const;
    void setBotCount(int count);

signals:
    void backRequested();
    /// @param opponents  toupies des bots, de 1 à kMaxBots
    void fightRequested(const toupie::Top &player, const QList<toupie::Top> &opponents,
                        toupie::Difficulty difficulty, const QString &stadiumId,
                        const toupie::MatchRules &rules);

private:
    void updatePreviews();
    void startFight();

    TopRepository &m_repository;
    QList<Top> m_tops;
    QComboBox *m_playerCombo = nullptr;
    QComboBox *m_opponentCombos[kMaxBots] = {};
    QWidget *m_opponentRows[kMaxBots] = {};
    TopChip *m_opponentChips[kMaxBots] = {};
    QButtonGroup *m_botCount = nullptr;
    QButtonGroup *m_difficulty = nullptr;
    QButtonGroup *m_stadiums = nullptr;
    QLabel *m_melee = nullptr;
    StadiumWidget *m_stadiumPreview = nullptr;
    RulesWidget *m_rules = nullptr;
    StatsWidget *m_playerPreview = nullptr;
};

} // namespace toupie
