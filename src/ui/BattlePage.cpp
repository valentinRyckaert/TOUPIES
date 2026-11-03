#include "ui/BattlePage.h"

#include "ui/ArenaWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace toupie {

BattlePage::BattlePage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 6);

    m_arena = new ArenaWidget(this);
    root->addWidget(m_arena, 1);

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(10, 0, 10, 0);
    auto *menuButton = new QPushButton(tr("Menu (Échap)"), this);
    auto *setupButton = new QPushButton(tr("Changer de toupies"), this);
    m_rematchButton = new QPushButton(tr("Revanche (R)"), this);
    m_score = new QLabel(this);
    bar->addWidget(menuButton);
    bar->addWidget(setupButton);
    bar->addStretch();
    bar->addWidget(m_score);
    bar->addStretch();
    bar->addWidget(m_rematchButton);
    root->addLayout(bar);

    // Les boutons ne doivent pas voler le focus clavier à l'arène.
    for (QPushButton *b : {menuButton, setupButton, m_rematchButton})
        b->setFocusPolicy(Qt::NoFocus);

    connect(menuButton, &QPushButton::clicked, this, &BattlePage::menuRequested);
    connect(setupButton, &QPushButton::clicked, this, &BattlePage::setupRequested);
    connect(m_rematchButton, &QPushButton::clicked, m_arena, &ArenaWidget::restart);
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this,
            &BattlePage::menuRequested);
    connect(m_arena, &ArenaWidget::matchFinished, this, &BattlePage::updateScore);
    connect(m_arena, &ArenaWidget::phaseChanged, this, [this](ArenaWidget::Phase phase) {
        m_rematchButton->setEnabled(phase == ArenaWidget::Phase::Finished);
    });
}

void BattlePage::startMatch(const Top &player, const Top &opponent, Difficulty difficulty)
{
    m_wins = m_losses = m_draws = 0;
    updateScore(-2);
    m_arena->startMatch(player, opponent, difficulty);
}

void BattlePage::stop()
{
    m_arena->stop();
}

void BattlePage::updateScore(int winner)
{
    if (winner == 0)
        ++m_wins;
    else if (winner == 1)
        ++m_losses;
    else if (winner == -1)
        ++m_draws;
    m_score->setText(tr("Victoires %1  ·  Défaites %2  ·  Nuls %3")
                         .arg(m_wins)
                         .arg(m_losses)
                         .arg(m_draws));
}

} // namespace toupie
