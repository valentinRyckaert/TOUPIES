#include "ui/TournamentSetupPage.h"

#include "core/TopRepository.h"
#include "game/Stadium.h"
#include "ui/RulesWidget.h"
#include "ui/TournamentSettingsWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace toupie {

TournamentSetupPage::TournamentSetupPage(TopRepository &repository, QWidget *parent)
    : QWidget(parent)
    , m_repository(repository)
{
    auto *outer = new QVBoxLayout(this);
    auto *title = new QLabel(tr("Tournois"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setObjectName(QStringLiteral("title"));
    outer->addWidget(title);

    auto *modeRow = new QHBoxLayout;
    m_solo = new QRadioButton(tr("Seul, contre des bots"), this);
    m_lan = new QRadioButton(tr("En multijoueur (réseau local, 2 à 4 joueurs humains)"), this);
    m_solo->setChecked(true);
    modeRow->addWidget(m_solo);
    modeRow->addWidget(m_lan);
    modeRow->addStretch();
    outer->addLayout(modeRow);

    // Contenu défilant : la page reste utilisable dans une petite fenêtre.
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    m_settings = new TournamentSettingsWidget(content);
    root->addWidget(m_settings);

    m_soloBox = new QGroupBox(tr("Votre tournoi solo"), content);
    auto *soloLayout = new QFormLayout(m_soloBox);
    for (int i = 0; i < 3; ++i) {
        auto *combo = new QComboBox(m_soloBox);
        m_topCombos.append(combo);
        soloLayout->addRow(tr("Toupie %1").arg(i + 1), combo);
    }
    m_stadium = new QComboBox(m_soloBox);
    for (const Stadium &stadium : Stadium::all())
        m_stadium->addItem(stadium.name, stadium.id);
    soloLayout->addRow(tr("Stadium"), m_stadium);
    root->addWidget(m_soloBox);
    m_rules = new RulesWidget(content);
    root->addWidget(m_rules);
    root->addStretch();

    auto *lanHint = new QLabel(
        tr("En réseau local, le stadium, la personnalisation et les toupies se choisissent dans "
           "le lobby du tournoi ; chaque joueur humain joue sur sa machine."),
        this);
    lanHint->setWordWrap(true);
    lanHint->setObjectName(QStringLiteral("hint"));
    outer->addWidget(lanHint);

    auto *buttons = new QHBoxLayout;
    auto *back = new QPushButton(tr("Retour au menu"), this);
    m_start = new QPushButton(this);
    m_start->setObjectName(QStringLiteral("accent"));
    m_start->setMinimumHeight(40);
    buttons->addWidget(back);
    buttons->addStretch();
    buttons->addWidget(m_start);
    outer->addLayout(buttons);

    connect(back, &QPushButton::clicked, this, &TournamentSetupPage::backRequested);
    connect(m_solo, &QRadioButton::toggled, this, &TournamentSetupPage::updateMode);
    connect(m_settings, &TournamentSettingsWidget::settingsChanged, this,
            &TournamentSetupPage::updateMode);
    connect(m_solo, &QRadioButton::toggled, lanHint, &QWidget::setHidden);
    lanHint->hide();
    connect(m_start, &QPushButton::clicked, this, [this] {
        if (m_lan->isChecked()) {
            emit lanRequested();
            return;
        }
        QList<Top> tops;
        const int needed = MatchSeries::topsFor(m_settings->settings().matchFormat);
        for (int i = 0; i < needed; ++i)
            tops.append(m_tops.value(m_topCombos.at(i)->currentIndex()));
        emit soloRequested(settings(), tops);
    });

    refresh();
}

TournamentSettings TournamentSetupPage::settings() const
{
    TournamentSettings s = m_settings->settings();
    s.stadiumId = m_stadium->currentData().toString();
    s.rules = m_rules->rules();
    return s;
}

void TournamentSetupPage::refresh()
{
    m_tops = m_repository.allTops();
    for (int i = 0; i < m_topCombos.size(); ++i) {
        QComboBox *combo = m_topCombos.at(i);
        const QString current = combo->currentText();
        combo->clear();
        for (const Top &top : m_tops)
            combo->addItem(QStringLiteral("%1  [%2]").arg(top.name(), toString(top.type())));
        const int found = combo->findText(current);
        combo->setCurrentIndex(found >= 0 ? found : std::min(i, int(m_tops.size()) - 1));
    }
    updateMode();
}

void TournamentSetupPage::updateMode()
{
    const bool solo = m_solo->isChecked();
    m_settings->setHumans(solo ? 1 : 2);
    m_soloBox->setVisible(solo);
    m_rules->setVisible(solo);
    const int needed = MatchSeries::topsFor(m_settings->settings().matchFormat);
    for (int i = 0; i < m_topCombos.size(); ++i) {
        m_topCombos.at(i)->setVisible(i < needed);
        if (auto *form = qobject_cast<QFormLayout *>(m_soloBox->layout()))
            form->setRowVisible(i, i < needed);
    }
    m_start->setText(solo ? tr("Lancer le tournoi") : tr("Créer ou rejoindre un tournoi en réseau…"));
}

} // namespace toupie
