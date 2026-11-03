#include "ui/RulesWidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace toupie {

namespace {
int toPercent(double value)
{
    return qRound(value * 100.0);
}
} // namespace

RulesWidget::RulesWidget(QWidget *parent)
    : QGroupBox(tr("Personnalisation"), parent)
{
    setCheckable(false);
    auto *root = new QVBoxLayout(this);
    auto *grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(4, 1);
    grid->setHorizontalSpacing(10);
    root->addLayout(grid);

    // Deux colonnes de réglages.
    m_speed = addPercentRow(grid, 0, tr("Vitesse"),
                            tr("Vitesse de déplacement : lancer, propulsion, dashs."), 50, 200,
                            false);
    m_knockback = addPercentRow(grid, 1, tr("Rebonds après un choc"),
                                tr("Force avec laquelle les toupies sont repoussées."), 0, 250,
                                false);
    m_wear = addPercentRow(grid, 2, tr("Usure"),
                           tr("Perte naturelle d'endurance au fil du temps."), 0, 250, false);
    m_damage = addPercentRow(grid, 3, tr("Dégâts"),
                             tr("Endurance perdue lors des chocs et des coups spéciaux."), 0, 250,
                             false);

    auto column = [&](int row, int col, QWidget *label, QWidget *widget, QWidget *value = nullptr) {
        grid->addWidget(label, row, col);
        grid->addWidget(widget, row, col + 1);
        if (value)
            grid->addWidget(value, row, col + 2);
    };
    auto addRight = [&](int row, const QString &label, const QString &tooltip, int minimum,
                        int maximum, bool zeroIsOff) {
        PercentRow r;
        auto *name = new QLabel(label, this);
        name->setToolTip(tooltip);
        r.slider = new QSlider(Qt::Horizontal, this);
        r.slider->setRange(minimum, maximum);
        r.slider->setSingleStep(5);
        r.slider->setPageStep(25);
        r.slider->setToolTip(tooltip);
        r.slider->setProperty("zeroIsOff", zeroIsOff);
        r.value = new QLabel(this);
        r.value->setMinimumWidth(78);
        column(row, 3, name, r.slider, r.value);
        connect(r.slider, &QSlider::valueChanged, this, &RulesWidget::emitChanged);
        return r;
    };
    m_dash = addRight(0, tr("Puissance des dashs"), tr("Bonus d'attaque pendant un dash."), 0, 300,
                      false);
    m_steering = addRight(1, tr("Pilotage (flèches)"),
                          tr("Force de la correction de trajectoire aux flèches. 0 = désactivé."),
                          0, 250, true);
    m_special = addRight(2, tr("Charge des coups spéciaux"),
                         tr("Vitesse de remplissage de la jauge. 0 = coups spéciaux désactivés."),
                         0, 300, true);

    m_endurance = new QSpinBox(this);
    m_endurance->setRange(30, 500);
    m_endurance->setSingleStep(10);
    m_endurance->setSuffix(tr(" pts"));
    m_endurance->setToolTip(tr("Endurance de départ de chaque toupie."));
    auto *enduranceLabel = new QLabel(tr("Endurance max"), this);
    grid->addWidget(enduranceLabel, 4, 0);
    grid->addWidget(m_endurance, 4, 1, 1, 2);

    m_timeLimit = new QComboBox(this);
    m_timeLimit->addItem(tr("Aucun"), 0);
    for (int seconds : {30, 60, 90, 120, 180, 300})
        m_timeLimit->addItem(tr("%1 s").arg(seconds), seconds);
    m_timeLimit->setToolTip(tr("Au-delà, la toupie qui a le plus d'endurance gagne. Sans limite, "
                               "la dernière toupie en jeu gagne."));
    grid->addWidget(new QLabel(tr("Temps maximal"), this), 3, 3);
    grid->addWidget(m_timeLimit, 3, 4, 1, 2);

    auto *bottom = new QHBoxLayout;
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_reset = new QPushButton(tr("Réinitialiser"), this);
    bottom->addWidget(m_summary, 1);
    bottom->addWidget(m_reset);
    root->addLayout(bottom);

    connect(m_endurance, &QSpinBox::valueChanged, this, &RulesWidget::emitChanged);
    connect(m_timeLimit, &QComboBox::currentIndexChanged, this, &RulesWidget::emitChanged);
    connect(m_reset, &QPushButton::clicked, this, [this] {
        setRules(MatchRules{});
        emit rulesChanged(rules());
    });

    setRules(MatchRules{});
}

RulesWidget::PercentRow RulesWidget::addPercentRow(QGridLayout *grid, int row, const QString &label,
                                                   const QString &tooltip, int minimum,
                                                   int maximum, bool zeroIsOff)
{
    PercentRow r;
    auto *name = new QLabel(label, this);
    name->setToolTip(tooltip);
    r.slider = new QSlider(Qt::Horizontal, this);
    r.slider->setRange(minimum, maximum);
    r.slider->setSingleStep(5);
    r.slider->setPageStep(25);
    r.slider->setToolTip(tooltip);
    r.slider->setProperty("zeroIsOff", zeroIsOff);
    r.value = new QLabel(this);
    r.value->setMinimumWidth(56);
    grid->addWidget(name, row, 0);
    grid->addWidget(r.slider, row, 1);
    grid->addWidget(r.value, row, 2);
    connect(r.slider, &QSlider::valueChanged, this, &RulesWidget::emitChanged);
    return r;
}

MatchRules RulesWidget::rules() const
{
    MatchRules r;
    r.speed = m_speed.slider->value() / 100.0;
    r.knockback = m_knockback.slider->value() / 100.0;
    r.wear = m_wear.slider->value() / 100.0;
    r.damage = m_damage.slider->value() / 100.0;
    r.dashPower = m_dash.slider->value() / 100.0;
    r.steering = m_steering.slider->value() / 100.0;
    r.specialCharge = m_special.slider->value() / 100.0;
    r.maxEndurance = m_endurance->value();
    r.timeLimit = m_timeLimit->currentData().toInt();
    return r;
}

void RulesWidget::setRules(const MatchRules &r)
{
    m_updating = true;
    m_speed.slider->setValue(toPercent(r.speed));
    m_knockback.slider->setValue(toPercent(r.knockback));
    m_wear.slider->setValue(toPercent(r.wear));
    m_damage.slider->setValue(toPercent(r.damage));
    m_dash.slider->setValue(toPercent(r.dashPower));
    m_steering.slider->setValue(toPercent(r.steering));
    m_special.slider->setValue(toPercent(r.specialCharge));
    m_endurance->setValue(qRound(r.maxEndurance));
    const int index = m_timeLimit->findData(qRound(r.timeLimit));
    if (index >= 0) {
        m_timeLimit->setCurrentIndex(index);
    } else {
        m_timeLimit->addItem(tr("%1 s").arg(qRound(r.timeLimit)), qRound(r.timeLimit));
        m_timeLimit->setCurrentIndex(m_timeLimit->count() - 1);
    }
    m_updating = false;
    updateLabels();
}

void RulesWidget::setReadOnly(bool readOnly)
{
    for (QWidget *w : {static_cast<QWidget *>(m_speed.slider), static_cast<QWidget *>(m_knockback.slider),
                       static_cast<QWidget *>(m_wear.slider), static_cast<QWidget *>(m_damage.slider),
                       static_cast<QWidget *>(m_dash.slider), static_cast<QWidget *>(m_steering.slider),
                       static_cast<QWidget *>(m_special.slider), static_cast<QWidget *>(m_endurance),
                       static_cast<QWidget *>(m_timeLimit)})
        w->setEnabled(!readOnly);
    m_reset->setVisible(!readOnly);
    setTitle(readOnly ? tr("Personnalisation (choisie par l'hôte)") : tr("Personnalisation"));
}

void RulesWidget::emitChanged()
{
    updateLabels();
    if (!m_updating)
        emit rulesChanged(rules());
}

void RulesWidget::updateLabels()
{
    for (const PercentRow *r : {&m_speed, &m_knockback, &m_wear, &m_damage, &m_dash, &m_steering,
                                &m_special}) {
        const int value = r->slider->value();
        const bool off = value == 0 && r->slider->property("zeroIsOff").toBool();
        r->value->setText(off ? tr("désactivé") : QStringLiteral("%1 %").arg(value));
    }
    m_summary->setText(rules().summary());
}

} // namespace toupie
