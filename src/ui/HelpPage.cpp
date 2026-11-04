#include "ui/HelpPage.h"

#include "ui/Controls.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace toupie {

HelpPage::HelpPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    auto *title = new QLabel(tr("Aide — commandes"), this);
    QFont font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.6);
    font.setBold(true);
    title->setFont(font);
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_text = new QLabel(scroll);
    m_text->setTextFormat(Qt::RichText);
    m_text->setWordWrap(true);
    m_text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setWidget(m_text);
    root->addWidget(scroll, 1);

    auto *buttons = new QHBoxLayout;
    auto *back = new QPushButton(tr("Retour au menu"), this);
    auto *settings = new QPushButton(tr("Modifier les touches…"), this);
    buttons->addWidget(back);
    buttons->addStretch();
    buttons->addWidget(settings);
    root->addLayout(buttons);
    connect(back, &QPushButton::clicked, this, &HelpPage::backRequested);
    connect(settings, &QPushButton::clicked, this, &HelpPage::settingsRequested);
    refresh();
}

void HelpPage::refresh()
{
    m_text->setText(Controls::instance().helpHtml());
}

} // namespace toupie
