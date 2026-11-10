#include "ui/BattlePage.h"

#include "ui/ArenaWidget.h"
#include "ui/Controls.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

namespace toupie {

BattlePage::BattlePage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 4);

    m_arena = new ArenaWidget(this);
    root->addWidget(m_arena, 1);

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(10, 0, 10, 0);
    m_leaveButton = new QPushButton(this);
    m_setupButton = new QPushButton(tr("Changer de toupies"), this);
    m_rematchButton = new QPushButton(this);
    m_specialButton = new QPushButton(tr("Coup spécial (E)"), this);
    m_specialButton->setProperty("variant", QStringLiteral("holo"));
    m_leaveButton->setProperty("variant", QStringLiteral("ghost"));
    m_leaveButton->setProperty("glyph", QStringLiteral("back"));
    m_rematchButton->setProperty("variant", QStringLiteral("primary"));
    m_score = new QLabel(this);
    m_score->setObjectName(QStringLiteral("muted"));
    bar->addWidget(m_leaveButton);
    bar->addWidget(m_setupButton);
    bar->addStretch();
    bar->addWidget(m_specialButton);
    bar->addWidget(m_score);
    bar->addStretch();
    bar->addWidget(m_rematchButton);
    root->addLayout(bar);

    // Les boutons ne doivent pas voler le focus clavier à l'arène.
    for (QPushButton *b : {m_leaveButton, m_setupButton, m_rematchButton, m_specialButton})
        b->setFocusPolicy(Qt::NoFocus);

    connect(m_leaveButton, &QPushButton::clicked, this, &BattlePage::leaveRequested);
    connect(m_setupButton, &QPushButton::clicked, this, &BattlePage::setupRequested);
    connect(m_rematchButton, &QPushButton::clicked, this, &BattlePage::rematchRequested);
    connect(m_specialButton, &QPushButton::clicked, m_arena, &ArenaWidget::trySpecial);
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this,
            &BattlePage::leaveRequested);
    connect(m_arena, &ArenaWidget::matchFinished, this, &BattlePage::onFinished);
    // État du bouton « coup spécial » (il peut être lancé au clavier).
    auto *refresh = new QTimer(this);
    connect(refresh, &QTimer::timeout, this, &BattlePage::updateButtons);
    refresh->start(200);
    connect(m_arena, &ArenaWidget::rematchRequested, this, [this] {
        if (m_mode == Mode::Solo || m_mode == Mode::LanHost)
            emit rematchRequested();
    });
}

BattlePage::~BattlePage()
{
    m_arena->setSession(nullptr);
}

void BattlePage::start(std::unique_ptr<MatchSession> session, Mode mode)
{
    m_arena->setSession(nullptr);
    m_session = std::move(session);
    m_mode = mode;
    m_wins = m_losses = m_draws = 0;

    if (m_session)
        connect(m_session.get(), &MatchSession::phaseChanged, this, &BattlePage::updateButtons);
    m_arena->setSession(m_session.get());
    updateButtons();
}

void BattlePage::stop()
{
    m_arena->setSession(nullptr);
    m_session.reset();
}

void BattlePage::onFinished(int winner)
{
    if (!m_session)
        return;
    const int local = m_session->localPlayer();
    if (winner == local)
        ++m_wins;
    else if (winner < 0)
        ++m_draws;
    else
        ++m_losses;
    updateButtons();
}

void BattlePage::updateButtons()
{
    const bool finished = m_session && m_session->state().phase == MatchPhase::Finished;
    m_setupButton->setVisible(m_mode == Mode::Solo);
    m_specialButton->setText(tr("Coup spécial (%1)").arg(Controls::instance().text(Action::Special)));
    m_specialButton->setVisible(m_session && m_session->state().config.specialsEnabled);
    m_specialButton->setEnabled(m_session && m_session->state().phase == MatchPhase::Fighting
                                && !m_session->state().spinners[m_session->localPlayer()].specialUsed);

    switch (m_mode) {
    case Mode::Solo:
        m_leaveButton->setText(tr("Menu (Échap)"));
        m_rematchButton->setText(tr("Revanche (%1)").arg(Controls::instance().text(Action::Rematch)));
        m_rematchButton->setVisible(true);
        m_rematchButton->setEnabled(finished);
        m_score->setText(tr("Victoires %1  ·  Défaites %2  ·  Nuls %3")
                             .arg(m_wins)
                             .arg(m_losses)
                             .arg(m_draws));
        break;
    case Mode::LanHost:
        m_leaveButton->setText(tr("Fermer le lobby"));
        m_rematchButton->setText(
            tr("Retour au lobby (%1)").arg(Controls::instance().text(Action::Rematch)));
        m_rematchButton->setVisible(true);
        m_rematchButton->setEnabled(finished);
        m_score->setText(finished ? tr("Combat terminé") : tr("Partie en réseau local (hôte)"));
        break;
    case Mode::LanClient:
        m_leaveButton->setText(tr("Quitter le lobby"));
        m_rematchButton->setVisible(false);
        m_score->setText(finished ? tr("En attente de l'hôte pour revenir au lobby…")
                                  : tr("Partie en réseau local"));
        break;
    case Mode::Tutorial:
        m_leaveButton->setText(tr("Quitter le tutoriel (Échap)"));
        m_rematchButton->setVisible(false);
        m_score->setText(tr("Tutoriel"));
        break;
    case Mode::Tournament:
        m_leaveButton->setText(tr("Abandonner le match"));
        m_rematchButton->setVisible(false);
        if (m_session && m_session->state().series) {
            const MatchSeries &series = *m_session->state().series;
            m_score->setText(series.format == MatchFormat::Classic
                                 ? tr("Match de tournoi · premier à 4 points")
                                 : tr("Match de tournoi · premier à 2 manches"));
        }
        break;
    }
}

} // namespace toupie
