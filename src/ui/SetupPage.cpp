#include "ui/SetupPage.h"

#include "core/TopRepository.h"
#include "game/Stadium.h"
#include "ui/RulesWidget.h"
#include "ui/StadiumPainter.h"
#include "ui/StatsWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QVBoxLayout>

namespace toupie {

namespace {
constexpr int kRandomOpponent = -1;
}

SetupPage::SetupPage(TopRepository &repository, QWidget *parent)
    : QWidget(parent)
    , m_repository(repository)
{
    auto *root = new QVBoxLayout(this);

    auto *title = new QLabel(tr("Préparer le combat"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *columns = new QHBoxLayout;
    root->addLayout(columns);

    auto *playerBox = new QGroupBox(tr("Votre toupie"), this);
    auto *playerLayout = new QVBoxLayout(playerBox);
    m_playerCombo = new QComboBox(playerBox);
    playerLayout->addWidget(m_playerCombo);
    m_playerPreview = new StatsWidget(playerBox);
    playerLayout->addWidget(m_playerPreview);
    columns->addWidget(playerBox);

    auto *opponentBox = new QGroupBox(tr("Toupie du bot"), this);
    auto *opponentLayout = new QVBoxLayout(opponentBox);
    m_opponentCombo = new QComboBox(opponentBox);
    opponentLayout->addWidget(m_opponentCombo);
    m_opponentPreview = new StatsWidget(opponentBox);
    opponentLayout->addWidget(m_opponentPreview);
    columns->addWidget(opponentBox);

    auto *form = new QFormLayout;
    m_difficultyCombo = new QComboBox(this);
    for (Difficulty d : {Difficulty::Easy, Difficulty::Normal, Difficulty::Hard})
        m_difficultyCombo->addItem(toString(d), static_cast<int>(d));
    m_difficultyCombo->setCurrentIndex(1);
    form->addRow(tr("Difficulté du bot"), m_difficultyCombo);
    m_stadiumCombo = new QComboBox(this);
    for (const Stadium &stadium : Stadium::all())
        m_stadiumCombo->addItem(stadium.name, stadium.id);
    form->addRow(tr("Stadium"), m_stadiumCombo);
    root->addLayout(form);
    m_stadiumPreview = new StadiumWidget(this);
    root->addWidget(m_stadiumPreview);
    m_rules = new RulesWidget(this);
    root->addWidget(m_rules);
    connect(m_stadiumCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (const auto stadium = Stadium::find(m_stadiumCombo->currentData().toString()))
            m_stadiumPreview->setStadium(*stadium);
    });

    auto *rules = new QLabel(
        tr("<b>Commandes</b> — Visez à la souris (la distance règle la force) ou avec "
           "← → / ↑ ↓, lancez d'un clic ou avec Entrée. En combat : <b>flèches</b> pour piloter "
           "légèrement, <b>Espace</b> ou clic pour dasher, <b>E</b> pour le coup spécial quand "
           "sa jauge est pleine. Une toupie éjectée ou à court d'endurance perd.<br>"
           "<b>Repères</b> — Attaque bat Endurance, Endurance bat Défense, Défense bat Attaque ; "
           "chaque stadium avantage légèrement certains types."),
        this);
    rules->setWordWrap(true);
    root->addWidget(rules);
    root->addStretch();

    auto *buttons = new QHBoxLayout;
    auto *backButton = new QPushButton(tr("Retour au menu"), this);
    auto *fightButton = new QPushButton(tr("Combattre !"), this);
    fightButton->setDefault(true);
    fightButton->setMinimumHeight(40);
    buttons->addWidget(backButton);
    buttons->addStretch();
    buttons->addWidget(fightButton);
    root->addLayout(buttons);

    connect(m_playerCombo, &QComboBox::currentIndexChanged, this, &SetupPage::updatePreviews);
    connect(m_opponentCombo, &QComboBox::currentIndexChanged, this, &SetupPage::updatePreviews);
    connect(backButton, &QPushButton::clicked, this, &SetupPage::backRequested);
    connect(fightButton, &QPushButton::clicked, this, &SetupPage::startFight);

    refresh();
}

void SetupPage::refresh()
{
    const QString playerName = m_playerCombo->currentText();
    const QString opponentName = m_opponentCombo->currentText();

    m_tops = m_repository.allTops();
    const int presetCount = m_tops.size() - m_repository.customTops().size();

    const QSignalBlocker b1(m_playerCombo);
    const QSignalBlocker b2(m_opponentCombo);
    m_playerCombo->clear();
    m_opponentCombo->clear();
    m_opponentCombo->addItem(tr("Aléatoire"), kRandomOpponent);

    for (int i = 0; i < m_tops.size(); ++i) {
        const Top &top = m_tops.at(i);
        const QString label = QStringLiteral("%1  [%2]%3")
                                  .arg(top.name(), toString(top.type()),
                                       i >= presetCount ? tr("  ★ perso") : QString());
        m_playerCombo->addItem(label, i);
        m_opponentCombo->addItem(label, i);
    }

    m_playerCombo->setCurrentIndex(qMax(0, m_playerCombo->findText(playerName)));
    m_opponentCombo->setCurrentIndex(qMax(0, m_opponentCombo->findText(opponentName)));
    updatePreviews();
}

void SetupPage::updatePreviews()
{
    const int player = m_playerCombo->currentData().toInt();
    if (player >= 0 && player < m_tops.size())
        m_playerPreview->setTop(m_tops.at(player));
    else
        m_playerPreview->setTop(std::nullopt);

    const int opponent = m_opponentCombo->currentData().toInt();
    if (opponent >= 0 && opponent < m_tops.size())
        m_opponentPreview->setTop(m_tops.at(opponent));
    else
        m_opponentPreview->setMystery(tr("Toupie choisie au hasard au lancement du combat"));
}

void SetupPage::startFight()
{
    const int player = m_playerCombo->currentData().toInt();
    if (player < 0 || player >= m_tops.size())
        return;

    int opponent = m_opponentCombo->currentData().toInt();
    if (opponent < 0 || opponent >= m_tops.size())
        opponent = QRandomGenerator::global()->bounded(static_cast<int>(m_tops.size()));

    const auto difficulty = static_cast<Difficulty>(m_difficultyCombo->currentData().toInt());
    emit fightRequested(m_tops.at(player), m_tops.at(opponent), difficulty,
                        m_stadiumCombo->currentData().toString(), m_rules->rules());
}

} // namespace toupie
