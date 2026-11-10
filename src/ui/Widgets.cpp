#include "ui/Widgets.h"
#include "ui/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
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

TopBar::TopBar(QPushButton *back, const QString &title, const QString &subtitle, QWidget *parent)
    : QWidget(parent)
{
    m_row = new QHBoxLayout(this);
    m_row->setContentsMargins(0, 0, 0, 22); // Le filet passe sous la rangée.
    m_row->setSpacing(10);
    back->setParent(this);
    m_row->addWidget(back);
    m_row->addSpacing(70); // Place des « /// ».
    m_title = Theme::createTitle(title, this);
    m_row->addWidget(m_title);
    m_row->addSpacing(18);
    m_subtitle = new QLabel(subtitle, this);
    m_subtitle->setObjectName(QStringLiteral("muted"));
    m_subtitle->setFont(Theme::bodyFont(19, QFont::Medium));
    m_row->addWidget(m_subtitle);
    m_row->addStretch();
}

void TopBar::setTitle(const QString &title)
{
    m_title->setText(title);
}

void TopBar::setSubtitle(const QString &subtitle)
{
    m_subtitle->setText(subtitle);
}

void TopBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRect title = m_title->geometry();
    // « /// » vertes, hautes comme les capitales du titre.
    QPen pen(Theme::kAccent, 3.0);
    p.setPen(pen);
    const double h = 26, top = title.center().y() - h / 2;
    for (int i = 0; i < 3; ++i) {
        const double x = title.left() - 50 + i * 12;
        p.drawLine(QPointF(x, top + h), QPointF(x + 9, top));
    }
    const double y = height() - 1.5;
    p.setPen(QPen(Theme::kBorder, 1.0));
    p.drawLine(QPointF(0, y), QPointF(width(), y));
    p.setPen(QPen(Theme::kCyan, 2.0));
    p.drawLine(QPointF(0, y), QPointF(std::min(380.0, width() * 0.25), y));
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
