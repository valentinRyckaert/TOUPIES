#include "ui/BuilderPage.h"

#include "core/PartCatalog.h"
#include "core/TopRepository.h"
#include "ui/StatsWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace toupie {

namespace {
constexpr PartKind kKinds[3] = {PartKind::Wheel, PartKind::Middle, PartKind::Tip};

QString partLabel(const Part &part)
{
    QString label = QStringLiteral("%1  (%2 / %3 / %4)")
                        .arg(part.name)
                        .arg(part.stats.attack)
                        .arg(part.stats.defense)
                        .arg(part.stats.endurance);
    if (part.kind == PartKind::Wheel)
        label += QStringLiteral(" — ") + toString(part.spin).toLower();
    return label;
}
} // namespace

BuilderPage::BuilderPage(const PartCatalog &catalog, TopRepository &repository, QWidget *parent)
    : QWidget(parent)
    , m_catalog(catalog)
    , m_repository(repository)
{
    auto *root = new QHBoxLayout(this);

    // --- Toupies sauvegardées --------------------------------------------------
    auto *savedBox = new QGroupBox(tr("Mes toupies"), this);
    auto *savedLayout = new QVBoxLayout(savedBox);
    m_list = new QListWidget(savedBox);
    savedLayout->addWidget(m_list);
    auto *listButtons = new QHBoxLayout;
    auto *newButton = new QPushButton(tr("Nouvelle"), savedBox);
    m_deleteButton = new QPushButton(tr("Supprimer"), savedBox);
    listButtons->addWidget(newButton);
    listButtons->addWidget(m_deleteButton);
    savedLayout->addLayout(listButtons);
    savedBox->setMaximumWidth(260);
    root->addWidget(savedBox);

    // --- Assemblage -------------------------------------------------------------
    auto *editor = new QVBoxLayout;
    root->addLayout(editor, 1);

    auto *title = new QLabel(tr("Assembler une toupie"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    editor->addWidget(title);

    auto *help = new QLabel(
        tr("Une toupie = une roue métallique + une partie intermédiaire + une pointe. "
           "Ses stats sont la somme de celles des pièces (attaque / défense / endurance). "
           "Toutes les pièces d'un même emplacement ont le même total de points : "
           "il s'agit de répartir, pas d'accumuler. La roue fixe le sens de rotation."),
        this);
    help->setWordWrap(true);
    editor->addWidget(help);

    auto *form = new QFormLayout;
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setMaxLength(24);
    m_nameEdit->setPlaceholderText(tr("Nom de la toupie"));
    form->addRow(tr("Nom"), m_nameEdit);
    for (int i = 0; i < 3; ++i)
        form->addRow(toString(kKinds[i]), createPartCombo(i));
    editor->addLayout(form);

    m_table = new QTableWidget(4, 4, this);
    m_table->setHorizontalHeaderLabels({tr("Pièce"), tr("Attaque"), tr("Défense"), tr("Endurance")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setFixedHeight(m_table->horizontalHeader()->sizeHint().height()
                            + 4 * m_table->verticalHeader()->defaultSectionSize() + 4);
    editor->addWidget(m_table);

    m_preview = new StatsWidget(this);
    editor->addWidget(m_preview);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    editor->addWidget(m_status);
    editor->addStretch();

    auto *buttons = new QHBoxLayout;
    auto *backButton = new QPushButton(tr("Retour au menu"), this);
    auto *saveButton = new QPushButton(tr("Enregistrer"), this);
    saveButton->setDefault(true);
    buttons->addWidget(backButton);
    buttons->addStretch();
    buttons->addWidget(saveButton);
    editor->addLayout(buttons);

    connect(m_nameEdit, &QLineEdit::textChanged, this, &BuilderPage::updatePreview);
    connect(newButton, &QPushButton::clicked, this, &BuilderPage::newTop);
    connect(m_deleteButton, &QPushButton::clicked, this, &BuilderPage::deleteSelected);
    connect(saveButton, &QPushButton::clicked, this, &BuilderPage::saveTop);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &BuilderPage::saveTop);
    connect(backButton, &QPushButton::clicked, this, &BuilderPage::backRequested);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        m_deleteButton->setEnabled(row >= 0);
        if (row >= 0 && row < m_repository.customTops().size())
            loadTop(m_repository.customTops().at(row));
    });

    refresh();
    newTop();
}

QComboBox *BuilderPage::createPartCombo(int kindIndex)
{
    auto *combo = new QComboBox(this);
    for (const Part &part : m_catalog.parts(kKinds[kindIndex]))
        combo->addItem(partLabel(part), part.id);
    connect(combo, &QComboBox::currentIndexChanged, this, &BuilderPage::updatePreview);
    m_partCombos[kindIndex] = combo;
    return combo;
}

void BuilderPage::refresh()
{
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const Top &top : m_repository.customTops())
        m_list->addItem(QStringLiteral("%1  [%2]").arg(top.name(), toString(top.type())));
    m_deleteButton->setEnabled(false);
}

Top BuilderPage::currentTop() const
{
    Top top;
    top.setName(m_nameEdit->text().trimmed());
    for (QComboBox *combo : m_partCombos) {
        if (const auto part = m_catalog.find(combo->currentData().toString()))
            top.setPart(*part);
    }
    return top;
}

void BuilderPage::updatePreview()
{
    const Top top = currentTop();

    auto setRow = [this](int row, const QString &name, const Stats &stats, bool bold) {
        const QString values[4] = {name, QString::number(stats.attack),
                                   QString::number(stats.defense),
                                   QString::number(stats.endurance)};
        for (int col = 0; col < 4; ++col) {
            auto *item = new QTableWidgetItem(values[col]);
            item->setTextAlignment(col == 0 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignCenter);
            if (bold) {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
            }
            m_table->setItem(row, col, item);
        }
    };
    for (int i = 0; i < 3; ++i) {
        const Part &part = top.part(kKinds[i]);
        setRow(i, part.name, part.stats, false);
    }
    setRow(3, tr("Total"), top.stats(), true);

    m_preview->setTop(top);
}

void BuilderPage::loadTop(const Top &top)
{
    m_nameEdit->setText(top.name());
    for (int i = 0; i < 3; ++i) {
        QComboBox *combo = m_partCombos[i];
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(qMax(0, combo->findData(top.part(kKinds[i]).id)));
    }
    m_status->clear();
    updatePreview();
}

void BuilderPage::newTop()
{
    {
        const QSignalBlocker blocker(m_list);
        m_list->setCurrentRow(-1);
    }
    m_deleteButton->setEnabled(false);
    m_nameEdit->clear();
    for (QComboBox *combo : m_partCombos) {
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(0);
    }
    m_status->clear();
    updatePreview();
    m_nameEdit->setFocus();
}

void BuilderPage::saveTop()
{
    const Top top = currentTop();
    if (top.name().isEmpty()) {
        m_status->setText(tr("Donnez un nom à votre toupie."));
        m_nameEdit->setFocus();
        return;
    }
    for (const Top &preset : m_catalog.presets()) {
        if (preset.name().compare(top.name(), Qt::CaseInsensitive) == 0) {
            m_status->setText(tr("« %1 » est le nom d'une toupie pré-assemblée.").arg(top.name()));
            return;
        }
    }
    if (!top.isValid()) {
        m_status->setText(tr("Assemblage incomplet."));
        return;
    }

    const bool existed = m_repository.contains(top.name());
    m_repository.upsert(top);
    if (!m_repository.save()) {
        QMessageBox::warning(this, tr("Sauvegarde impossible"),
                             tr("Impossible d'écrire le fichier :\n%1").arg(m_repository.filePath()));
        return;
    }

    refresh();
    for (int i = 0; i < m_repository.customTops().size(); ++i) {
        if (m_repository.customTops().at(i).name().compare(top.name(), Qt::CaseInsensitive) == 0) {
            const QSignalBlocker blocker(m_list);
            m_list->setCurrentRow(i);
            m_deleteButton->setEnabled(true);
        }
    }
    m_status->setText(existed ? tr("« %1 » mise à jour.").arg(top.name())
                              : tr("« %1 » enregistrée (type %2).")
                                    .arg(top.name(), toString(top.type()).toLower()));
    emit topsChanged();
}

void BuilderPage::deleteSelected()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_repository.customTops().size())
        return;
    const QString name = m_repository.customTops().at(row).name();
    if (QMessageBox::question(this, tr("Supprimer"), tr("Supprimer la toupie « %1 » ?").arg(name))
        != QMessageBox::Yes)
        return;

    m_repository.remove(name);
    m_repository.save();
    refresh();
    newTop();
    m_status->setText(tr("« %1 » supprimée.").arg(name));
    emit topsChanged();
}

} // namespace toupie
