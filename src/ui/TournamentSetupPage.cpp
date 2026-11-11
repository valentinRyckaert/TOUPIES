#include "ui/TournamentSetupPage.h"
#include "ui/Theme.h"

#include "core/PartCatalog.h"
#include "core/TopRepository.h"
#include "game/Tournament.h"
#include "game/Stadium.h"
#include "ui/RulesWidget.h"
#include "ui/TournamentSettingsWidget.h"
#include "ui/Widgets.h"

#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace toupie {

namespace {
QLabel *pageTitle(const QString &text, QWidget *parent)
{
    auto *title = Theme::createTitle(text, parent);
    return title;
}
} // namespace

TournamentSetupPage::TournamentSetupPage(TopRepository &repository, QWidget *parent)
    : QWidget(parent)
    , m_repository(repository)
{
    auto *outer = new QVBoxLayout(this);
    m_steps = new QStackedWidget(this);
    outer->addWidget(m_steps);

    // --- Étape 1 : solo ou réseau local -----------------------------------------
    auto *choice = new QWidget(m_steps);
    auto *choiceLayout = new QVBoxLayout(choice);
    choiceLayout->addWidget(pageTitle(tr("Tournois"), choice));
    choiceLayout->addStretch();
    auto *solo = createChoiceCard(tr("Tournoi solo"),
                                  tr("Vous contre des bots : ligue ou élimination directe, "
                                     "de 4 à 32 participants."),
                                  choice);
    auto *lan = createChoiceCard(tr("Tournoi en réseau local"),
                                 tr("De 2 à 4 joueurs humains, chacun sur sa machine, complétés "
                                    "par des bots."),
                                 choice);
    solo->setProperty("glyph", QStringLiteral("trophy"));
    lan->setProperty("glyph", QStringLiteral("lan"));
    // Visible quand un tournoi solo a été sauvegardé (texte mis à jour par refresh()).
    m_resume = createChoiceCard(tr("Reprendre le tournoi solo"), QString(), choice);
    m_resume->setProperty("glyph", QStringLiteral("play"));
    choiceLayout->addWidget(m_resume);
    choiceLayout->addWidget(solo);
    choiceLayout->addWidget(lan);
    choiceLayout->addStretch();
    auto *choiceButtons = new QHBoxLayout;
    auto *back = new QPushButton(tr("Retour au menu"), choice);
    back->setProperty("variant", QStringLiteral("ghost"));
    back->setProperty("glyph", QStringLiteral("back"));
    choiceButtons->addWidget(back);
    choiceButtons->addStretch();
    choiceLayout->addLayout(choiceButtons);
    m_steps->addWidget(choice);

    // --- Étape 2 : tournoi solo -----------------------------------------------------
    auto *soloPage = new QWidget(m_steps);
    auto *soloLayout = new QVBoxLayout(soloPage);
    soloLayout->addWidget(pageTitle(tr("Tournoi solo"), soloPage));
    m_settings = new TournamentSettingsWidget(soloPage);
    soloLayout->addWidget(m_settings);

    auto *playerBox = new QGroupBox(tr("Vos toupies et le stadium"), soloPage);
    m_soloForm = new QFormLayout(playerBox);
    for (int i = 0; i < 3; ++i) {
        auto *combo = new QComboBox(playerBox);
        m_topCombos.append(combo);
        m_soloForm->addRow(tr("Toupie %1").arg(i + 1), combo);
    }
    m_stadium = new QComboBox(playerBox);
    for (const Stadium &stadium : Stadium::all())
        m_stadium->addItem(stadium.name, stadium.id);
    m_soloForm->addRow(tr("Stadium"), m_stadium);
    auto *rulesRow = new QHBoxLayout;
    m_rulesSummary = new QLabel(playerBox);
    m_rulesSummary->setObjectName(QStringLiteral("muted"));
    auto *rulesButton = new QPushButton(tr("Modifier…"), playerBox);
    rulesRow->addWidget(m_rulesSummary, 1);
    rulesRow->addWidget(rulesButton);
    m_soloForm->addRow(tr("Personnalisation"), rulesRow);
    soloLayout->addWidget(playerBox);
    soloLayout->addStretch();

    m_rules = new RulesWidget;
    QDialog *rulesPopup = createPopup(this, tr("Personnalisation"), m_rules);

    auto *soloButtons = new QHBoxLayout;
    auto *previous = new QPushButton(tr("Retour"), soloPage);
    previous->setProperty("variant", QStringLiteral("ghost"));
    previous->setProperty("glyph", QStringLiteral("back"));
    auto *start = new QPushButton(tr("Lancer le tournoi"), soloPage);
    start->setObjectName(QStringLiteral("accent"));
    start->setMinimumHeight(40);
    soloButtons->addWidget(previous);
    soloButtons->addStretch();
    soloButtons->addWidget(start);
    soloLayout->addLayout(soloButtons);
    m_steps->addWidget(soloPage);

    connect(back, &QPushButton::clicked, this, &TournamentSetupPage::backRequested);
    connect(solo, &QPushButton::clicked, this, [this] { m_steps->setCurrentIndex(1); });
    connect(lan, &QPushButton::clicked, this, &TournamentSetupPage::lanRequested);
    connect(m_resume, &QPushButton::clicked, this, &TournamentSetupPage::resumeRequested);
    connect(previous, &QPushButton::clicked, this, [this] { m_steps->setCurrentIndex(0); });
    connect(rulesButton, &QPushButton::clicked, rulesPopup, &QDialog::open);
    connect(m_rules, &RulesWidget::rulesChanged, this,
            [this] { m_rulesSummary->setText(m_rules->rules().summary()); });
    connect(m_settings, &TournamentSettingsWidget::settingsChanged, this,
            &TournamentSetupPage::updateTops);
    connect(start, &QPushButton::clicked, this, [this] {
        QList<Top> tops;
        const int needed = MatchSeries::topsFor(m_settings->settings().matchFormat);
        for (int i = 0; i < needed; ++i)
            tops.append(m_tops.value(m_topCombos.at(i)->currentIndex()));
        emit soloRequested(settings(), tops);
    });

    m_rulesSummary->setText(m_rules->rules().summary());
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
    m_steps->setCurrentIndex(0);
    const auto saved = SavedTournament::load(PartCatalog::instance());
    m_resume->setVisible(saved.has_value());
    if (saved) {
        m_resume->setText(tr("Reprendre le tournoi solo") + QLatin1Char('\n')
                          + tr("%1 · match %2 · %3 participants — %4, là où vous l'avez laissé.")
                                .arg(toString(saved->settings().format),
                                     toString(saved->settings().matchFormat).toLower())
                                .arg(saved->participants().size())
                                .arg(saved->roundName(saved->currentRound())));
    }
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
    updateTops();
}

void TournamentSetupPage::updateTops()
{
    const int needed = MatchSeries::topsFor(m_settings->settings().matchFormat);
    for (int i = 0; i < m_topCombos.size(); ++i)
        m_soloForm->setRowVisible(i, i < needed);
}

} // namespace toupie
