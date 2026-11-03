#include "ui/MenuPage.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace toupie {

MenuPage::MenuPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->addStretch(2);

    auto *title = new QLabel(tr("Toupie Arena"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 3.0);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *subtitle = new QLabel(tr("Assemblez votre toupie et affrontez le bot dans le stadium."),
                                this);
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);
    layout->addSpacing(24);

    auto addButton = [&](const QString &text, auto signal) {
        auto *button = new QPushButton(text, this);
        button->setMinimumSize(260, 46);
        QFont f = button->font();
        f.setPointSizeF(f.pointSizeF() * 1.2);
        button->setFont(f);
        connect(button, &QPushButton::clicked, this, signal);
        layout->addWidget(button, 0, Qt::AlignHCenter);
        return button;
    };

    addButton(tr("Jouer une partie"), &MenuPage::playRequested)->setDefault(true);
    addButton(tr("Assembler une toupie"), &MenuPage::builderRequested);
    addButton(tr("Quitter"), &MenuPage::quitRequested);

    layout->addStretch(3);
}

} // namespace toupie
