#include "ui/Widgets.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace toupie {

SidePanel::SidePanel(const QString &title, QWidget *content, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_content(content)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_toggle = new QToolButton(this);
    m_toggle->setObjectName(QStringLiteral("sideToggle"));
    m_toggle->setCheckable(true);
    m_toggle->setChecked(true);
    m_toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_toggle);
    content->setParent(this);
    layout->addWidget(content, 1);
    connect(m_toggle, &QToolButton::toggled, this, &SidePanel::setExpanded);
    setExpanded(true);
}

void SidePanel::setExpanded(bool expanded)
{
    m_toggle->setChecked(expanded);
    m_content->setVisible(expanded);
    m_toggle->setText(expanded ? QStringLiteral("◀  %1").arg(m_title) : QStringLiteral("▶"));
    m_toggle->setToolTip(expanded ? tr("Replier") : m_title);
    setMaximumWidth(expanded ? QWIDGETSIZE_MAX : m_toggle->sizeHint().width() + 4);
}

bool SidePanel::isExpanded() const
{
    return m_content->isVisible();
}

QDialog *createPopup(QWidget *parent, const QString &title, QWidget *content)
{
    auto *dialog = new QDialog(parent);
    dialog->setWindowTitle(title);
    auto *layout = new QVBoxLayout(dialog);
    content->setParent(dialog);
    layout->addWidget(content);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);
    return dialog;
}

QPushButton *createChoiceCard(const QString &title, const QString &description, QWidget *parent)
{
    auto *card = new QPushButton(QStringLiteral("%1\n%2").arg(title, description), parent);
    card->setObjectName(QStringLiteral("card"));
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(96);
    return card;
}

std::optional<QList<int>> chooseTopOrder(QWidget *parent, const QList<Top> &tops)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Ordre de vos toupies"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *hint = new QLabel(QObject::tr("La 1re toupie joue la manche 1, la 2e la manche 2, la 3e "
                                        "la manche 3, puis on recommence. Votre adversaire ne voit "
                                        "pas votre ordre."),
                            &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *row = new QHBoxLayout;
    auto *list = new QListWidget(&dialog);
    for (int i = 0; i < tops.size(); ++i) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  [%2]").arg(tops.at(i).name(), toString(tops.at(i).type())), list);
        item->setData(Qt::UserRole, i);
    }
    list->setCurrentRow(0);
    row->addWidget(list, 1);
    auto *moves = new QVBoxLayout;
    auto *up = new QPushButton(QObject::tr("▲ Monter"), &dialog);
    auto *down = new QPushButton(QObject::tr("▼ Descendre"), &dialog);
    moves->addWidget(up);
    moves->addWidget(down);
    moves->addStretch();
    row->addLayout(moves);
    layout->addLayout(row);

    // Échange de deux lignes (pas de glisser-déposer : il peut perdre des éléments).
    auto move = [list](int delta) {
        const int from = list->currentRow();
        const int to = from + delta;
        if (from < 0 || to < 0 || to >= list->count())
            return;
        list->insertItem(to, list->takeItem(from));
        list->setCurrentRow(to);
    };
    QObject::connect(up, &QPushButton::clicked, list, [move] { move(-1); });
    QObject::connect(down, &QPushButton::clicked, list, [move] { move(1); });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Valider et jouer"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;
    QList<int> order;
    for (int i = 0; i < list->count(); ++i)
        order.append(list->item(i)->data(Qt::UserRole).toInt());
    return order;
}

} // namespace toupie
