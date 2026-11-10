#include "ui/SetupPage.h"

#include "core/TopRepository.h"
#include "game/Stadium.h"
#include "ui/Controls.h"
#include "ui/RulesWidget.h"
#include "ui/StadiumPainter.h"
#include "ui/StatsWidget.h"
#include "ui/Theme.h"
#include "ui/TopPainter.h"
#include "ui/Widgets.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <optional>

namespace toupie {

/// Vignette compacte d'une toupie de bot (ou « ? » si elle est tirée au hasard).
class TopChip : public QWidget
{
public:
    explicit TopChip(QWidget *parent)
        : QWidget(parent)
    {
        setFixedHeight(54);
        setMinimumWidth(190);
        m_timer = startTimer(33);
    }

    void setTop(const std::optional<Top> &top)
    {
        m_top = top;
        update();
    }

protected:
    void timerEvent(QTimerEvent *event) override
    {
        if (event->timerId() == m_timer && m_top && isVisible()) {
            m_angle += spinSign(m_top->spin()) * 0.25;
            update();
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(26, height() / 2.0);
        const QRectF text(58, 4, width() - 62, height() - 8);
        if (!m_top) {
            QPen dashed(Theme::kBorderBright, 1.4);
            dashed.setDashPattern({3, 4});
            p.setPen(dashed);
            p.setBrush(Theme::kField);
            p.drawEllipse(c, 21, 21);
            p.setFont(Theme::displayFont(20));
            p.setPen(Theme::kCyan);
            p.drawText(QRectF(c.x() - 21, c.y() - 21, 42, 42), Qt::AlignCenter, QStringLiteral("?"));
            p.setFont(Theme::bodyFont(15, QFont::DemiBold));
            p.setPen(Theme::kMuted);
            p.drawText(text, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                       SetupPage::tr("Tirée au hasard"));
            return;
        }
        TopPainter::paint(p, *m_top, c, 22, m_angle);
        QFont name = Theme::displayFont(15);
        name.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p.setFont(name);
        p.setPen(Theme::kText);
        p.drawText(QRectF(text.left(), text.top(), text.width(), 22), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_top->name().toUpper(), Qt::ElideRight, int(text.width())));
        const QString type = toString(m_top->type());
        Theme::paintTag(p, QRectF(text.left(), text.top() + 25, 40 + type.size() * 9.5, 19), type,
                        TopPainter::typeColor(m_top->type()));
    }

private:
    std::optional<Top> m_top;
    double m_angle = 0.0;
    int m_timer = 0;
};

namespace {

constexpr int kRandomOpponent = -1;

/// Carte de choix d'un stadium : miniature, nom et nom de code.
class StadiumCard : public QAbstractButton
{
public:
    StadiumCard(const Stadium &stadium, QWidget *parent)
        : QAbstractButton(parent)
        , m_stadium(stadium)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setText(stadium.name);
        setToolTip(StadiumPainter::describeTraits(stadium));
        setFixedHeight(96);
        setMinimumWidth(150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override { return {190, 96}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        const QPainterPath card = Theme::chamfer(r, 14);
        const bool on = isChecked();
        const bool hover = underMouse();
        p.fillPath(card, on ? QColor(0x16, 0x21, 0x4f) : QColor(0x0e, 0x0b, 0x30));
        if (on) {
            p.save();
            p.setClipPath(card);
            p.setPen(QPen(QColor(0x3d, 0xff, 0x8b, 80), 8.0));
            p.drawPath(card);
            p.restore();
        }
        p.setPen(QPen(on ? Theme::kAccent : hover || hasFocus() ? Theme::kBorderBright : Theme::kBorder,
                      on ? 2.2 : 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(card);

        const BattleConfig config = m_stadium.apply(BattleConfig{});
        const double thumb = std::min(r.height() * 0.34, 32.0);
        StadiumPainter::paint(p, m_stadium, m_stadium.geometry(config),
                              QPointF(r.left() + 12 + thumb, r.center().y()), thumb);
        const QRectF text(r.left() + 24 + thumb * 2, r.top() + 10, r.width() - 34 - thumb * 2,
                          r.height() - 20);
        QFont name = Theme::displayFont(14);
        name.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        p.setFont(name);
        p.setPen(Theme::kText);
        p.drawText(QRectF(text.left(), text.top() + 6, text.width(), 22), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_stadium.name.toUpper(), Qt::ElideRight, int(text.width())));
        QFont code = Theme::displayFont(10);
        code.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p.setFont(code);
        p.setPen(QColor::fromRgb(m_stadium.accent));
        p.drawText(QRectF(text.left(), text.top() + 30, text.width(), 18), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_stadium.codename.toUpper(), Qt::ElideRight,
                                              int(text.width())));
        if (on)
            Theme::paintIcon(p, QStringLiteral("check"), QRectF(r.right() - 26, r.bottom() - 26, 16, 16),
                             Theme::kAccent);
    }

private:
    Stadium m_stadium;
};

/// « VS » holographique entre les deux camps.
class VersusBadge : public QWidget
{
public:
    explicit VersusBadge(QWidget *parent)
        : QWidget(parent)
    {
        setFixedWidth(96);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(width() / 2.0, height() / 2.0);
        p.setPen(QPen(Theme::kBorder, 1.0));
        p.drawLine(QPointF(c.x(), 20), QPointF(c.x(), c.y() - 60));
        p.drawLine(QPointF(c.x(), c.y() + 60), QPointF(c.x(), height() - 20));
        const QRectF box(c.x() - 42, c.y() - 38, 84, 76);
        p.fillPath(Theme::chamfer(box, 18), Theme::kField);
        QLinearGradient border(box.topLeft(), box.bottomRight());
        border.setColorAt(0.0, Theme::kCyan);
        border.setColorAt(1.0, Theme::kMagenta);
        p.setPen(QPen(QBrush(border), 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(Theme::chamfer(box, 18));
        const QFont font = Theme::displayFont(38, true);
        p.setFont(font);
        QColor magenta = Theme::kMagenta, cyan = Theme::kCyan;
        magenta.setAlpha(190);
        cyan.setAlpha(190);
        p.setPen(magenta);
        p.drawText(box.translated(-3, 0), Qt::AlignCenter, QStringLiteral("VS"));
        p.setPen(cyan);
        p.drawText(box.translated(3, 1), Qt::AlignCenter, QStringLiteral("VS"));
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, QStringLiteral("VS"));
        Theme::paintSlashes(p, QPointF(c.x() - 12, box.bottom() + 10), 10, Theme::kAccent);
    }
};

QPushButton *segment(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumWidth(74);
    return button;
}

QLabel *fieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("muted"));
    label->setMinimumWidth(86);
    return label;
}

} // namespace

SetupPage::SetupPage(TopRepository &repository, QWidget *parent)
    : QWidget(parent)
    , m_repository(repository)
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);

    auto *header = new QHBoxLayout;
    header->addWidget(Theme::createTitle(tr("Préparer le combat"), this));
    auto *subtitle = new QLabel(tr("Solo contre 1 à 3 bots"), this);
    subtitle->setObjectName(QStringLiteral("muted"));
    header->addSpacing(12);
    header->addWidget(subtitle, 0, Qt::AlignBottom);
    header->addStretch();
    root->addLayout(header);

    auto *columns = new QHBoxLayout;
    root->addLayout(columns);

    // --- Joueur -----------------------------------------------------------------
    auto *playerBox = new QGroupBox(tr("Votre toupie"), this);
    auto *playerLayout = new QVBoxLayout(playerBox);
    m_playerCombo = new QComboBox(playerBox);
    playerLayout->addWidget(m_playerCombo);
    m_playerPreview = new StatsWidget(playerBox);
    playerLayout->addWidget(m_playerPreview, 1);
    columns->addWidget(playerBox, 1);
    columns->addWidget(new VersusBadge(this));

    // --- Bots ---------------------------------------------------------------------
    auto *opponentBox = new QGroupBox(tr("Adversaires"), this);
    auto *opponentLayout = new QVBoxLayout(opponentBox);
    auto *countRow = new QHBoxLayout;
    countRow->addWidget(fieldLabel(tr("Bots"), opponentBox));
    m_botCount = new QButtonGroup(this);
    for (int n = 1; n <= kMaxBots; ++n) {
        QPushButton *button = segment(QString::number(n), opponentBox);
        button->setObjectName(QStringLiteral("bots%1").arg(n));
        m_botCount->addButton(button, n);
        countRow->addWidget(button);
    }
    m_melee = new QLabel(opponentBox);
    m_melee->setObjectName(QStringLiteral("muted"));
    countRow->addSpacing(8);
    countRow->addWidget(m_melee, 1);
    opponentLayout->addLayout(countRow);

    for (int i = 0; i < kMaxBots; ++i) {
        m_opponentRows[i] = new QWidget(opponentBox);
        auto *row = new QHBoxLayout(m_opponentRows[i]);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(fieldLabel(tr("Bot %1").arg(i + 1), m_opponentRows[i]));
        m_opponentCombos[i] = new QComboBox(m_opponentRows[i]);
        m_opponentCombos[i]->setMinimumWidth(200);
        row->addWidget(m_opponentCombos[i], 1);
        m_opponentChips[i] = new TopChip(m_opponentRows[i]);
        row->addWidget(m_opponentChips[i], 1);
        opponentLayout->addWidget(m_opponentRows[i]);
        connect(m_opponentCombos[i], &QComboBox::currentIndexChanged, this, &SetupPage::updatePreviews);
    }

    auto *difficultyRow = new QHBoxLayout;
    difficultyRow->addWidget(fieldLabel(tr("Difficulté"), opponentBox));
    m_difficulty = new QButtonGroup(this);
    for (Difficulty d : {Difficulty::Easy, Difficulty::Normal, Difficulty::Hard}) {
        QPushButton *button = segment(toString(d), opponentBox);
        m_difficulty->addButton(button, static_cast<int>(d));
        difficultyRow->addWidget(button);
    }
    m_difficulty->button(static_cast<int>(Difficulty::Normal))->setChecked(true);
    difficultyRow->addStretch();
    opponentLayout->addLayout(difficultyRow);
    opponentLayout->addStretch();
    columns->addWidget(opponentBox, 1);

    // --- Stadium ------------------------------------------------------------------
    auto *stadiumBox = new QGroupBox(tr("Stadium"), this);
    auto *stadiumLayout = new QVBoxLayout(stadiumBox);
    auto *cards = new QHBoxLayout;
    m_stadiums = new QButtonGroup(this);
    const QList<Stadium> &stadiums = Stadium::all();
    for (int i = 0; i < stadiums.size(); ++i) {
        auto *card = new StadiumCard(stadiums.at(i), stadiumBox);
        m_stadiums->addButton(card, i);
        cards->addWidget(card);
    }
    m_stadiums->button(0)->setChecked(true);
    stadiumLayout->addLayout(cards);
    m_stadiumPreview = new StadiumWidget(stadiumBox);
    m_stadiumPreview->setFixedHeight(96);
    stadiumLayout->addWidget(m_stadiumPreview);
    root->addWidget(stadiumBox);
    connect(m_stadiums, &QButtonGroup::idClicked, this, [this](int id) {
        m_stadiumPreview->setStadium(Stadium::all().value(id, Stadium::classic()));
    });

    m_rules = new RulesWidget;
    QDialog *rulesPopup = createPopup(this, tr("Personnalisation"), m_rules);
    auto *rules = new QLabel(this);
    rules->setWordWrap(true);
    rules->setMaximumWidth(560);
    QDialog *helpPopup = createPopup(this, tr("Commandes et repères"), rules);
    root->addStretch();

    auto *buttons = new QHBoxLayout;
    auto *backButton = new QPushButton(tr("Retour au menu"), this);
    backButton->setProperty("variant", QStringLiteral("ghost"));
    backButton->setProperty("glyph", QStringLiteral("back"));
    auto *rulesButton = new QPushButton(tr("Personnalisation…"), this);
    rulesButton->setProperty("glyph", QStringLiteral("sliders"));
    auto *helpButton = new QPushButton(tr("Commandes…"), this);
    helpButton->setProperty("variant", QStringLiteral("ghost"));
    helpButton->setProperty("glyph", QStringLiteral("help"));
    rulesButton->setToolTip(m_rules->rules().summary());
    connect(m_rules, &RulesWidget::rulesChanged, rulesButton, [this, rulesButton] {
        rulesButton->setText(m_rules->rules().isDefault() ? tr("Personnalisation…")
                                                          : tr("Personnalisation ✱…"));
        rulesButton->setToolTip(m_rules->rules().summary());
    });
    connect(rulesButton, &QPushButton::clicked, rulesPopup, &QDialog::open);
    connect(helpButton, &QPushButton::clicked, helpPopup, [this, rules, helpPopup] {
        // Touches actuelles (page Paramètres) et repères de jeu.
        rules->setText(Controls::instance().helpHtml()
                       + tr("<p><b>Repères</b> — Une toupie éjectée ou à court d'endurance perd. "
                            "Attaque bat Endurance, Endurance bat Défense, Défense bat Attaque ; "
                            "chaque stadium avantage légèrement certains types.</p>"));
        helpPopup->open();
    });
    auto *fightButton = new QPushButton(tr("Combattre !"), this);
    fightButton->setObjectName(QStringLiteral("accent"));
    fightButton->setProperty("glyph", QStringLiteral("play"));
    fightButton->setDefault(true);
    fightButton->setMinimumHeight(50);
    fightButton->setMinimumWidth(260);
    buttons->addWidget(backButton);
    buttons->addStretch();
    buttons->addWidget(helpButton);
    buttons->addWidget(rulesButton);
    buttons->addWidget(fightButton);
    root->addLayout(buttons);

    connect(m_playerCombo, &QComboBox::currentIndexChanged, this, &SetupPage::updatePreviews);
    connect(m_botCount, &QButtonGroup::idClicked, this, &SetupPage::setBotCount);
    connect(backButton, &QPushButton::clicked, this, &SetupPage::backRequested);
    connect(fightButton, &QPushButton::clicked, this, &SetupPage::startFight);

    refresh();
    setBotCount(1);
}

int SetupPage::botCount() const
{
    return std::max(1, m_botCount->checkedId());
}

void SetupPage::setBotCount(int count)
{
    count = std::clamp(count, 1, kMaxBots);
    m_botCount->button(count)->setChecked(true);
    for (int i = 0; i < kMaxBots; ++i)
        m_opponentRows[i]->setVisible(i < count);
    m_melee->setText(count == 1 ? tr("Duel à 2 toupies")
                                : tr("Mêlée à %1 toupies : le dernier en jeu gagne").arg(count + 1));
}

void SetupPage::refresh()
{
    const QString playerName = m_playerCombo->currentText();
    QString opponentNames[kMaxBots];
    for (int i = 0; i < kMaxBots; ++i)
        opponentNames[i] = m_opponentCombos[i]->currentText();

    m_tops = m_repository.allTops();
    const int presetCount = m_tops.size() - m_repository.customTops().size();

    const QSignalBlocker b1(m_playerCombo);
    m_playerCombo->clear();
    for (QComboBox *combo : m_opponentCombos) {
        const QSignalBlocker b(combo);
        combo->clear();
        combo->addItem(tr("Aléatoire"), kRandomOpponent);
    }

    for (int i = 0; i < m_tops.size(); ++i) {
        const Top &top = m_tops.at(i);
        const QString label = QStringLiteral("%1  [%2]%3")
                                  .arg(top.name(), toString(top.type()),
                                       i >= presetCount ? tr("  ★ perso") : QString());
        m_playerCombo->addItem(label, i);
        for (QComboBox *combo : m_opponentCombos) {
            const QSignalBlocker b(combo);
            combo->addItem(label, i);
        }
    }

    m_playerCombo->setCurrentIndex(qMax(0, m_playerCombo->findText(playerName)));
    for (int i = 0; i < kMaxBots; ++i) {
        const QSignalBlocker b(m_opponentCombos[i]);
        m_opponentCombos[i]->setCurrentIndex(qMax(0, m_opponentCombos[i]->findText(opponentNames[i])));
    }
    updatePreviews();
}

void SetupPage::updatePreviews()
{
    const int player = m_playerCombo->currentData().toInt();
    if (player >= 0 && player < m_tops.size())
        m_playerPreview->setTop(m_tops.at(player));
    else
        m_playerPreview->setTop(std::nullopt);

    for (int i = 0; i < kMaxBots; ++i) {
        const int opponent = m_opponentCombos[i]->currentData().toInt();
        m_opponentChips[i]->setTop(opponent >= 0 && opponent < m_tops.size()
                                       ? std::optional<Top>(m_tops.at(opponent))
                                       : std::nullopt);
    }
}

void SetupPage::startFight()
{
    const int player = m_playerCombo->currentData().toInt();
    if (player < 0 || player >= m_tops.size())
        return;

    QList<Top> opponents;
    for (int i = 0; i < botCount(); ++i) {
        int opponent = m_opponentCombos[i]->currentData().toInt();
        if (opponent < 0 || opponent >= m_tops.size())
            opponent = QRandomGenerator::global()->bounded(static_cast<int>(m_tops.size()));
        opponents.append(m_tops.at(opponent));
    }
    const auto difficulty = static_cast<Difficulty>(m_difficulty->checkedId());
    const Stadium stadium = Stadium::all().value(m_stadiums->checkedId(), Stadium::classic());
    emit fightRequested(m_tops.at(player), opponents, difficulty, stadium.id, m_rules->rules());
}

} // namespace toupie
