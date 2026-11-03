#pragma once

#include "core/Top.h"
#include "game/BotController.h"

#include <QWidget>

class QLabel;
class QPushButton;

namespace toupie {

class ArenaWidget;

/// Écran de combat : l'arène et les actions disponibles autour.
class BattlePage : public QWidget
{
    Q_OBJECT

public:
    explicit BattlePage(QWidget *parent = nullptr);

    void startMatch(const Top &player, const Top &opponent, Difficulty difficulty);
    void stop();

signals:
    void setupRequested();
    void menuRequested();

private:
    void updateScore(int winner);

    ArenaWidget *m_arena = nullptr;
    QLabel *m_score = nullptr;
    QPushButton *m_rematchButton = nullptr;
    int m_wins = 0;
    int m_losses = 0;
    int m_draws = 0;
};

} // namespace toupie
