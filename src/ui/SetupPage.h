#pragma once

#include "core/Top.h"
#include "game/BotController.h"

#include <QList>
#include <QWidget>

class QComboBox;

namespace toupie {

class StatsWidget;
class TopRepository;

/// Préparation d'un combat : choix de sa toupie, de celle du bot et de la difficulté.
class SetupPage : public QWidget
{
    Q_OBJECT

public:
    explicit SetupPage(TopRepository &repository, QWidget *parent = nullptr);

    /// Recharge la liste des toupies disponibles en conservant la sélection.
    void refresh();

signals:
    void backRequested();
    void fightRequested(const toupie::Top &player, const toupie::Top &opponent,
                        toupie::Difficulty difficulty);

private:
    void updatePreviews();
    void startFight();

    TopRepository &m_repository;
    QList<Top> m_tops;
    QComboBox *m_playerCombo = nullptr;
    QComboBox *m_opponentCombo = nullptr;
    QComboBox *m_difficultyCombo = nullptr;
    StatsWidget *m_playerPreview = nullptr;
    StatsWidget *m_opponentPreview = nullptr;
};

} // namespace toupie
