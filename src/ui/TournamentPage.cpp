#include "ui/TournamentPage.h"

#include "game/Stadium.h"
#include "net/TournamentSession.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace toupie {

TournamentPage::TournamentPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    m_title = Theme::createTitle(QString(), this);
    root->addWidget(m_title);
    m_subtitle = new QLabel(this);
    m_subtitle->setWordWrap(true);
    root->addWidget(m_subtitle);

    auto *columns = new QHBoxLayout;
    m_rounds = new QTreeWidget;
    m_rounds->setHeaderHidden(true);
    m_rounds->setSelectionMode(QAbstractItemView::NoSelection);
    m_rounds->setFocusPolicy(Qt::NoFocus);
    columns->addWidget(new SidePanel(tr("Tours et résultats"), m_rounds, this), 1);

    m_standings = new QTableWidget(0, 7, this);
    m_standings->setHorizontalHeaderLabels(
        {tr("#"), tr("Participant"), tr("J"), tr("V"), tr("D"), tr("Pts"), tr("Diff.")});
    m_standings->verticalHeader()->setVisible(false);
    m_standings->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_standings->setSelectionMode(QAbstractItemView::NoSelection);
    m_standings->setFocusPolicy(Qt::NoFocus);
    m_standings->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_standings->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    columns->addWidget(m_standings, 1);
    root->addLayout(columns, 1);

    m_matchBox = new QGroupBox(tr("Votre match"), this);
    auto *matchLayout = new QVBoxLayout(m_matchBox);
    m_matchLabel = new QLabel(m_matchBox);
    m_matchLabel->setWordWrap(true);
    matchLayout->addWidget(m_matchLabel);
    m_ready = new QPushButton(tr("Prêt — jouer le match"), m_matchBox);
    m_ready->setObjectName(QStringLiteral("accent"));
    m_ready->setMinimumHeight(38);
    matchLayout->addWidget(m_ready);
    root->addWidget(m_matchBox);

    auto *buttons = new QHBoxLayout;
    m_leave = new QPushButton(tr("Quitter le tournoi"), this);
    m_leave->setProperty("variant", QStringLiteral("danger"));
    m_leave->setProperty("glyph", QStringLiteral("quit"));
    m_returnToLobby = new QPushButton(tr("Retour au lobby"), this);
    buttons->addWidget(m_leave);
    buttons->addStretch();
    buttons->addWidget(m_returnToLobby);
    root->addLayout(buttons);

    connect(m_leave, &QPushButton::clicked, this, &TournamentPage::leaveRequested);
    connect(m_returnToLobby, &QPushButton::clicked, this, &TournamentPage::returnToLobbyRequested);
    connect(m_ready, &QPushButton::clicked, this, [this] {
        if (!m_controller)
            return;
        const Tournament &t = m_controller->tournament();
        const QList<Top> tops = t.participants().value(m_controller->localParticipant()).tops;
        QList<int> order{0};
        if (t.settings().matchFormat == MatchFormat::Classic) {
            const auto chosen = chooseTopOrder(this, tops);
            if (!chosen || !m_controller)
                return;
            order = *chosen;
        }
        m_controller->submitOrder(order);
    });
}

void TournamentPage::setController(net::TournamentController *controller, bool host)
{
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);
    m_controller = controller;
    m_host = host;
    if (m_controller)
        connect(m_controller, &net::TournamentController::changed, this, &TournamentPage::rebuild);
    rebuild();
}

QString TournamentPage::participantLabel(int participant) const
{
    if (!m_controller || participant < 0)
        return tr("exempt");
    const Participant &p = m_controller->tournament().participants().at(participant);
    if (participant == m_controller->localParticipant())
        return p.name == tr("Vous") ? p.name : tr("%1 (vous)").arg(p.name);
    return p.isBot() ? tr("%1 (%2)").arg(p.name, toString(p.level)) : p.name;
}

void TournamentPage::rebuild()
{
    if (!m_controller)
        return;
    const Tournament &t = m_controller->tournament();
    const TournamentSettings &settings = t.settings();
    const int local = m_controller->localParticipant();
    const int current = t.currentRound();

    m_title->setText(tr("%1 · match %2").arg(toString(settings.format),
                                             toString(settings.matchFormat).toLower()));
    const QString stadium = Stadium::find(settings.stadiumId).value_or(Stadium::classic()).name;
    if (t.isFinished()) {
        const int champion = t.champion();
        m_subtitle->setText(champion == local
                                ? tr("Tournoi terminé : vous êtes champion·ne !")
                                : tr("Tournoi terminé — champion·ne : %1")
                                      .arg(participantLabel(champion)));
    } else {
        m_subtitle->setText(tr("%1 · %2 participants · stadium %3")
                                .arg(t.roundName(current))
                                .arg(t.participants().size())
                                .arg(stadium));
    }

    // Tours : résultats, matchs en cours et à venir.
    m_rounds->clear();
    QTreeWidgetItem *currentItem = nullptr;
    for (int r = 0; r < t.rounds().size(); ++r) {
        auto *roundItem = new QTreeWidgetItem(m_rounds, {t.roundName(r)});
        const QList<TournamentMatch> &round = t.rounds().at(r);
        for (int i = 0; i < round.size(); ++i) {
            const TournamentMatch &m = round.at(i);
            QString text;
            if (m.isBye()) {
                text = tr("%1 — exempt(e) ce tour").arg(participantLabel(m.a));
            } else if (m.played) {
                text = QStringLiteral("%1   %2 – %3   %4")
                           .arg(participantLabel(m.a))
                           .arg(m.score[0])
                           .arg(m.score[1])
                           .arg(participantLabel(m.b));
            } else if (const auto *live = r == current ? m_controller->liveOf(i) : nullptr;
                       live && live->series.rounds > 0) {
                text = tr("%1   %2 – %3   %4   (en cours)")
                           .arg(participantLabel(m.a))
                           .arg(live->series.score[0])
                           .arg(live->series.score[1])
                           .arg(participantLabel(m.b));
            } else {
                text = tr("%1   contre   %2").arg(participantLabel(m.a), participantLabel(m.b));
            }
            auto *item = new QTreeWidgetItem(roundItem, {text});
            if (local >= 0 && (m.a == local || m.b == local))
                item->setForeground(0, Theme::kAccent);
        }
        if (r == current) {
            roundItem->setExpanded(true);
            currentItem = roundItem;
        }
    }
    if (currentItem)
        m_rounds->scrollToItem(currentItem, QAbstractItemView::PositionAtTop);

    // Classement (ligue).
    const bool league = settings.format == TournamentFormat::League;
    m_standings->setVisible(league);
    if (league) {
        const QList<Standing> table = t.standings();
        m_standings->setRowCount(static_cast<int>(table.size()));
        for (int row = 0; row < table.size(); ++row) {
            const Standing &s = table.at(row);
            const QStringList cells{QString::number(row + 1), participantLabel(s.participant),
                                    QString::number(s.played), QString::number(s.won),
                                    QString::number(s.lost), QString::number(s.points),
                                    QString::number(s.points - s.against)};
            for (int col = 0; col < cells.size(); ++col) {
                auto *item = new QTableWidgetItem(cells.at(col));
                if (s.participant == local)
                    item->setForeground(Theme::kAccent);
                m_standings->setItem(row, col, item);
            }
        }
    }

    // Match du joueur local.
    const int match = m_controller->localMatch();
    const bool ready = m_controller->localReady();
    const bool classic = settings.matchFormat == MatchFormat::Classic;
    m_ready->setVisible(match >= 0);
    m_ready->setEnabled(!ready);
    m_ready->setText(classic ? tr("Choisir l'ordre de mes toupies et jouer…")
                             : tr("Prêt — jouer le match"));
    if (match >= 0) {
        const TournamentMatch &m = t.rounds().at(current).at(match);
        const int opponent = m.a == local ? m.b : m.a;
        m_matchLabel->setText(ready ? tr("Contre %1 — en attente de votre adversaire…")
                                          .arg(participantLabel(opponent))
                                    : tr("%1 : vous affrontez %2.")
                                          .arg(t.roundName(current), participantLabel(opponent)));
    } else if (t.isFinished()) {
        m_matchLabel->setText(tr("Le tournoi est terminé."));
    } else if (local < 0) {
        m_matchLabel->setText(tr("Vous suivez le tournoi."));
    } else if (settings.format == TournamentFormat::Knockout && t.matchOf(local) < 0
               && std::none_of(t.rounds().at(current).begin(), t.rounds().at(current).end(),
                               [local](const TournamentMatch &m) { return m.a == local; })) {
        m_matchLabel->setText(tr("Vous êtes éliminé·e. La suite du tournoi se joue sans vous."));
    } else {
        m_matchLabel->setText(tr("Rien à jouer pour vous dans ce tour : en attente des autres "
                                 "matchs…"));
    }
    m_returnToLobby->setVisible(m_host && t.isFinished());
}

} // namespace toupie
