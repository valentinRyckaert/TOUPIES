#include "ui/SettingsPage.h"

#include "ui/Controls.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

namespace toupie {

namespace {
/// Case d'association : un clic la met en attente, la touche ou le bouton de
/// souris suivant devient l'entrée (Échap annule, Retour arrière efface).
class BindingButton : public QPushButton
{
public:
    explicit BindingButton(QWidget *parent)
        : QPushButton(parent)
    {
        setMinimumWidth(150);
        connect(this, &QPushButton::clicked, this, [this] {
            m_capturing = true;
            setText(QObject::tr("Appuyez sur une touche…"));
        });
    }

    std::function<void(int)> captured; ///< Code de l'entrée (0 : effacer).
    std::function<void()> cancelled;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (!m_capturing) {
            QPushButton::keyPressEvent(event);
            return;
        }
        if (event->isAutoRepeat())
            return;
        m_capturing = false;
        const int key = event->key();
        if (key == Qt::Key_Escape)
            cancelled();
        else
            captured(key == Qt::Key_Backspace || key == Qt::Key_Delete ? 0 : key);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (!m_capturing) {
            QPushButton::mousePressEvent(event);
            return;
        }
        m_capturing = false;
        captured(Controls::mouseCode(event->button()));
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        if (m_capturing) {
            m_capturing = false;
            cancelled();
        }
        QPushButton::focusOutEvent(event);
    }

private:
    bool m_capturing = false;
};
} // namespace

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    auto *title = new QLabel(tr("Paramètres"), this);
    QFont font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.6);
    font.setBold(true);
    title->setFont(font);
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);

    auto *box = new QGroupBox(tr("Touches et souris"), this);
    auto *grid = new QGridLayout(box);
    grid->addWidget(new QLabel(tr("<b>Action</b>"), box), 0, 0);
    grid->addWidget(new QLabel(tr("<b>Entrée 1</b>"), box), 0, 1);
    grid->addWidget(new QLabel(tr("<b>Entrée 2</b>"), box), 0, 2);
    for (int a = 0; a < kActionCount; ++a) {
        const auto action = static_cast<Action>(a);
        grid->addWidget(new QLabel(Controls::actionName(action), box), a + 1, 0);
        for (int slot = 0; slot < Controls::kSlots; ++slot) {
            auto *button = new BindingButton(box);
            m_buttons[a][slot] = button;
            grid->addWidget(button, a + 1, slot + 1);
            button->cancelled = [this] { refresh(); };
            button->captured = [this, action, slot](int code) {
                const auto previous = Controls::instance().bind(action, slot, code);
                refresh();
                if (previous)
                    m_status->setText(tr("« %1 » servait à « %2 » : elle a été retirée de cette "
                                         "action.")
                                          .arg(Controls::codeText(code),
                                               Controls::actionName(*previous)));
            };
        }
    }
    grid->setColumnStretch(0, 1);
    root->addWidget(box);

    auto *hint = new QLabel(
        tr("Cliquez sur une case puis appuyez sur une touche ou un bouton de la souris. Échap "
           "annule, Retour arrière efface la case. La souris vise toujours pendant la visée, et "
           "Échap reste la touche pour quitter."),
        this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("hint"));
    root->addWidget(hint);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    root->addStretch();

    auto *buttons = new QHBoxLayout;
    auto *back = new QPushButton(tr("Retour au menu"), this);
    auto *reset = new QPushButton(tr("Rétablir les touches par défaut"), this);
    buttons->addWidget(back);
    buttons->addStretch();
    buttons->addWidget(reset);
    root->addLayout(buttons);
    connect(back, &QPushButton::clicked, this, &SettingsPage::backRequested);
    connect(reset, &QPushButton::clicked, this, [this] {
        Controls::instance().reset();
        refresh();
    });
    refresh();
}

void SettingsPage::refresh()
{
    m_status->clear();
    const Controls &controls = Controls::instance();
    for (int a = 0; a < kActionCount; ++a) {
        for (int slot = 0; slot < Controls::kSlots; ++slot)
            m_buttons[a][slot]->setText(
                Controls::codeText(controls.binding(static_cast<Action>(a), slot)));
    }
}

} // namespace toupie
