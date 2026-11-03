#pragma once

#include "game/MatchRules.h"

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

namespace toupie {

/// Section « Personnalisation » : réglages de la partie (vitesse, rebonds,
/// usure, endurance, durée…). En lecture seule pour les clients d'un lobby.
class RulesWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit RulesWidget(QWidget *parent = nullptr);

    MatchRules rules() const;
    void setRules(const MatchRules &rules);
    void setReadOnly(bool readOnly);

signals:
    void rulesChanged(const toupie::MatchRules &rules);

private:
    struct PercentRow
    {
        QSlider *slider = nullptr;
        QLabel *value = nullptr;
    };

    PercentRow addPercentRow(QGridLayout *grid, int row, const QString &label,
                             const QString &tooltip, int minimum, int maximum, bool zeroIsOff);
    void emitChanged();
    void updateLabels();

    PercentRow m_speed;
    PercentRow m_knockback;
    PercentRow m_wear;
    PercentRow m_damage;
    PercentRow m_dash;
    PercentRow m_steering;
    QCheckBox *m_specials = nullptr;
    QSpinBox *m_endurance = nullptr;
    QComboBox *m_timeLimit = nullptr;
    QPushButton *m_reset = nullptr;
    QLabel *m_summary = nullptr;
    bool m_updating = false;
};

} // namespace toupie
