#include "ui/BattlePage.h"

#include "ui/ArenaWidget.h"
#include "ui/Controls.h"
#include "ui/Theme.h"

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

    // En haut à gauche, par-dessus l'arène (maquette) : menu / abandon et changement de toupies.
    m_leaveButton = new QPushButton(m_arena);
    m_leaveButton->setProperty("variant", QStringLiteral("ghost"));
    m_leaveButton->setProperty("key", tr("Échap"));
    m_setupButton = new QPushButton(tr("Changer de toupies"), m_arena);
    m_setupButton->setProperty("variant", QStringLiteral("ghost"));
    auto *overlay = new QVBoxLayout(m_arena);
    overlay->setContentsMargins(12, 6, 12, 6);
    auto *top = new QHBoxLayout;
    top->addWidget(m_leaveButton);
    top->addWidget(m_setupButton);
    top->addStretch();
    overlay->addLayout(top);
    overlay->addStretch();

    // En bas : rappel des commandes et revanche.
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(10, 0, 10, 0);
    m_hints = new QLabel(this);
    m_hints->setObjectName(QStringLiteral("muted"));
    m_hints->setFont(Theme::bodyFont(16, QFont::DemiBold));
    m_rematchButton = new QPushButton(this);
    m_rematchButton->setProperty("variant", QStringLiteral("primary"));
    bar->addStretch();
    bar->addWidget(m_hints);
    bar->addStretch();
    bar->addWidget(m_rematchButton);
    root->addLayout(bar);

    // Les boutons ne doivent pas voler le focus clavier à l'arène.
    for (QPushButton *b : {m_leaveButton, m_setupButton, m_rematchButton})
        b->setFocusPolicy(Qt::NoFocus);

    connect(m_leaveButton, &QPushButton::clicked, this, &BattlePage::leaveRequested);
    connect(m_setupButton, &QPushButton::clicked, this, &BattlePage::setupRequested);
    connect(m_rematchButton, &QPushButton::clicked, this, &BattlePage::rematchRequested);
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this,
            &BattlePage::leaveRequested);
    connect(m_arena, &ArenaWidget::matchFinished, this, &BattlePage::onFinished);
    // Rappel des commandes (réglables) et états qui changent pendant le combat.
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
    const Controls &controls = Controls::instance();
    QString hints = tr("%1 %2 %3 %4 piloter · %5 dash")
                        .arg(Controls::codeText(controls.binding(Action::SteerUp, 0)),
                             Controls::codeText(controls.binding(Action::SteerDown, 0)),
                             Controls::codeText(controls.binding(Action::SteerLeft, 0)),
                             Controls::codeText(controls.binding(Action::SteerRight, 0)),
                             Controls::codeText(controls.binding(Action::Dash, 0)));
    if (m_session && m_session->state().config.specialsEnabled)
        hints += tr(" · %1 coup spécial").arg(Controls::codeText(controls.binding(Action::Special, 0)));
    m_hints->setText(hints);
    QString caption;

    switch (m_mode) {
    case Mode::Solo:
        m_leaveButton->setText(tr("Menu"));
        m_rematchButton->setText(tr("Revanche (%1)").arg(Controls::instance().text(Action::Rematch)));
        m_rematchButton->setVisible(true);
        m_rematchButton->setEnabled(finished);
        caption = tr("Victoires %1  ·  Défaites %2  ·  Nuls %3").arg(m_wins).arg(m_losses).arg(m_draws);
        break;
    case Mode::LanHost:
        m_leaveButton->setText(tr("Fermer le lobby"));
        m_rematchButton->setText(
            tr("Retour au lobby (%1)").arg(Controls::instance().text(Action::Rematch)));
        m_rematchButton->setVisible(true);
        m_rematchButton->setEnabled(finished);
        caption = finished ? tr("Combat terminé") : tr("Partie en réseau local (hôte)");
        break;
    case Mode::LanClient:
        m_leaveButton->setText(tr("Quitter le lobby"));
        m_rematchButton->setVisible(false);
        caption = finished ? tr("En attente de l'hôte pour revenir au lobby…") : tr("Partie en réseau local");
        break;
    case Mode::Tutorial:
        m_leaveButton->setText(tr("Quitter le tutoriel"));
        m_rematchButton->setVisible(false);
        caption = tr("Tutoriel");
        break;
    case Mode::Tournament:
        m_leaveButton->setText(tr("Abandonner le match"));
        m_rematchButton->setVisible(false);
        if (m_session && m_session->state().series) {
            const MatchSeries &series = *m_session->state().series;
            caption = series.format == MatchFormat::Classic ? tr("Match de tournoi · premier à 4 points")
                                                            : tr("Match de tournoi · premier à 2 manches");
        }
        break;
    }
    m_arena->setCaption(caption);
}

} // namespace toupie
