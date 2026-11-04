#include "ui/TournamentSettingsWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace toupie {

namespace {
QComboBox *difficultyCombo(QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    for (Difficulty d : {Difficulty::Easy, Difficulty::Normal, Difficulty::Hard})
        combo->addItem(toString(d), static_cast<int>(d));
    return combo;
}
} // namespace

TournamentSettingsWidget::TournamentSettingsWidget(QWidget *parent)
    : QGroupBox(tr("Tournoi"), parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_format = new QComboBox(this);
    m_format->addItem(toString(TournamentFormat::League), static_cast<int>(TournamentFormat::League));
    m_format->addItem(toString(TournamentFormat::Knockout),
                      static_cast<int>(TournamentFormat::Knockout));
    m_format->setToolTip(tr("Ligue : chacun rencontre chacun une fois, les manches ou points gagnés "
                            "s'additionnent au classement.\nÉlimination directe : le perdant de "
                            "chaque match est éliminé."));
    form->addRow(tr("Format"), m_format);

    m_match = new QComboBox(this);
    m_match->addItem(toString(MatchFormat::Express), static_cast<int>(MatchFormat::Express));
    m_match->addItem(toString(MatchFormat::Classic), static_cast<int>(MatchFormat::Classic));
    form->addRow(tr("Type de match"), m_match);
    m_matchHelp = new QLabel(this);
    m_matchHelp->setWordWrap(true);
    m_matchHelp->setObjectName(QStringLiteral("hint"));
    form->addRow(QString(), m_matchHelp);

    auto *countRow = new QHBoxLayout;
    m_participants = new QSpinBox(this);
    m_participants->setRange(Tournament::kMinParticipants, Tournament::kMaxParticipants);
    m_split = new QLabel(this);
    countRow->addWidget(m_participants);
    countRow->addWidget(m_split, 1);
    form->addRow(tr("Participants"), countRow);

    m_allBots = new QComboBox(this);
    m_allBots->addItem(tr("Régler tous les bots…"), -1);
    for (Difficulty d : {Difficulty::Easy, Difficulty::Normal, Difficulty::Hard})
        m_allBots->addItem(tr("Tous : %1").arg(toString(d)), static_cast<int>(d));
    form->addRow(tr("Niveau des bots"), m_allBots);
    layout->addLayout(form);

    m_bots = new QTableWidget(0, 2, this);
    m_bots->setHorizontalHeaderLabels({tr("Bot"), tr("Niveau")});
    m_bots->verticalHeader()->setVisible(false);
    m_bots->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_bots->setSelectionMode(QAbstractItemView::NoSelection);
    m_bots->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_bots->setFixedHeight(150);
    layout->addWidget(m_bots);

    connect(m_format, &QComboBox::currentIndexChanged, this, &TournamentSettingsWidget::edited);
    connect(m_match, &QComboBox::currentIndexChanged, this, &TournamentSettingsWidget::edited);
    connect(m_participants, &QSpinBox::valueChanged, this, &TournamentSettingsWidget::edited);
    connect(m_allBots, &QComboBox::activated, this, [this] {
        const int level = m_allBots->currentData().toInt();
        m_allBots->setCurrentIndex(0);
        if (level < 0)
            return;
        for (Difficulty &d : m_settings.bots)
            d = static_cast<Difficulty>(level);
        refresh();
        emit settingsChanged(m_settings);
    });

    m_settings.fitBots(m_humans);
    refresh();
}

void TournamentSettingsWidget::setSettings(const TournamentSettings &settings)
{
    m_settings = settings;
    m_settings.fitBots(m_humans);
    refresh();
}

void TournamentSettingsWidget::setHumans(int humans)
{
    m_humans = humans;
    m_settings.participants = std::max(m_settings.participants, humans);
    m_settings.fitBots(humans);
    refresh();
}

void TournamentSettingsWidget::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    refresh();
}

void TournamentSettingsWidget::edited()
{
    if (m_updating)
        return;
    m_settings.format = static_cast<TournamentFormat>(m_format->currentData().toInt());
    m_settings.matchFormat = static_cast<MatchFormat>(m_match->currentData().toInt());
    m_settings.participants = m_participants->value();
    m_settings.fitBots(m_humans);
    refresh();
    emit settingsChanged(m_settings);
}

void TournamentSettingsWidget::refresh()
{
    m_updating = true;
    m_format->setCurrentIndex(m_format->findData(static_cast<int>(m_settings.format)));
    m_match->setCurrentIndex(m_match->findData(static_cast<int>(m_settings.matchFormat)));
    m_participants->setMinimum(std::max(Tournament::kMinParticipants, m_humans));
    m_participants->setValue(m_settings.participants);
    m_split->setText(tr("dont %n humain(s)", nullptr, m_humans)
                     + tr(" et %n bot(s)", nullptr, static_cast<int>(m_settings.bots.size())));
    m_matchHelp->setText(m_settings.matchFormat == MatchFormat::Express
                             ? tr("Une toupie pour tout le tournoi ; le premier à gagner 2 manches "
                                  "remporte le match.")
                             : tr("Trois toupies jouées dans l'ordre choisi avant chaque match ; "
                                  "premier à 4 points (arrêt de la toupie adverse : 1 pt, "
                                  "éjection : 2 pts)."));

    m_bots->setRowCount(static_cast<int>(m_settings.bots.size()));
    for (int i = 0; i < m_settings.bots.size(); ++i) {
        m_bots->setItem(i, 0, new QTableWidgetItem(tr("Bot %1").arg(i + 1)));
        auto *combo = qobject_cast<QComboBox *>(m_bots->cellWidget(i, 1));
        if (!combo) {
            combo = difficultyCombo(m_bots);
            m_bots->setCellWidget(i, 1, combo);
            connect(combo, &QComboBox::currentIndexChanged, this, [this, i, combo] {
                if (m_updating || i >= m_settings.bots.size())
                    return;
                m_settings.bots[i] = static_cast<Difficulty>(combo->currentData().toInt());
                emit settingsChanged(m_settings);
            });
        }
        combo->setCurrentIndex(combo->findData(static_cast<int>(m_settings.bots.at(i))));
        combo->setEnabled(!m_readOnly);
    }
    for (QWidget *w : {static_cast<QWidget *>(m_format), static_cast<QWidget *>(m_match),
                       static_cast<QWidget *>(m_participants), static_cast<QWidget *>(m_allBots)})
        w->setEnabled(!m_readOnly);
    m_updating = false;
}

} // namespace toupie
