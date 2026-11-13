#include "ui/SettingsPage.h"

#include "game/MatchRules.h"
#include "ui/Theme.h"

#include "ui/Audio.h"
#include "ui/Controls.h"
#include "ui/Language.h"

#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
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
    auto *title = Theme::createTitle(tr("Paramètres"), this);
    root->addWidget(title);

    auto *languageBox = new QGroupBox(tr("Langue"), this);
    auto *languageRow = new QHBoxLayout(languageBox);
    m_language = new QComboBox(languageBox);
    for (const QString &code : Language::available())
        m_language->addItem(Language::nativeName(code), code);
    m_language->setCurrentIndex(m_language->findData(Language::current()));
    m_language->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    languageRow->addWidget(m_language);
    auto *languageHint = new QLabel(tr("Le jeu redémarre pour changer de langue."), languageBox);
    languageHint->setObjectName(QStringLiteral("hint"));
    languageRow->addWidget(languageHint, 1);
    root->addWidget(languageBox);
    connect(m_language, &QComboBox::activated, this, [this] {
        const QString code = m_language->currentData().toString();
        if (code == Language::current())
            return;
        if (QMessageBox::question(this, tr("Langue"),
                                  tr("Redémarrer le jeu maintenant pour passer en %1 ?")
                                      .arg(Language::nativeName(code)))
            != QMessageBox::Yes) {
            m_language->setCurrentIndex(m_language->findData(Language::current()));
            return;
        }
        Language::setCurrent(code);
        emit restartRequested();
    });

    // --- Son : chaque musique et les effets s'activent et se règlent séparément -------
    auto *soundBox = new QGroupBox(tr("Son"), this);
    auto *soundGrid = new QGridLayout(soundBox);
    const std::pair<Audio::Channel, QString> channels[] = {
        {Audio::MenuMusic, tr("Musique des menus")},
        {Audio::BattleMusic, tr("Musique des combats")},
        {Audio::Effects, tr("Effets sonores")},
    };
    for (int row = 0; row < Audio::kChannels; ++row) {
        const Audio::Channel channel = channels[row].first;
        auto *enabled = new QCheckBox(channels[row].second, soundBox);
        enabled->setChecked(Audio::instance().isEnabled(channel));
        auto *slider = new QSlider(Qt::Horizontal, soundBox);
        slider->setRange(0, 100);
        slider->setValue(Audio::instance().volume(channel));
        slider->setEnabled(enabled->isChecked());
        auto *value = new QLabel(tr("%1 %").arg(slider->value()), soundBox);
        value->setMinimumWidth(50);
        soundGrid->addWidget(enabled, row, 0);
        soundGrid->addWidget(slider, row, 1);
        soundGrid->addWidget(value, row, 2);
        connect(enabled, &QCheckBox::toggled, this, [channel, slider](bool on) {
            Audio::instance().setEnabled(channel, on);
            slider->setEnabled(on);
        });
        connect(slider, &QSlider::valueChanged, this, [channel, value](int percent) {
            Audio::instance().setVolume(channel, percent);
            value->setText(tr("%1 %").arg(percent));
        });
        if (channel == Audio::Effects) // Aperçu du volume des effets.
            connect(slider, &QSlider::sliderReleased, this, [] { Audio::instance().play({Sfx::Hit}); });
    }
    soundGrid->setColumnStretch(1, 1);
    root->addWidget(soundBox);

    // --- Duels : préférence globale, valeur par défaut des règles de partie ------------
    auto *duelBox = new QGroupBox(tr("Duels"), this);
    auto *duelLayout = new QHBoxLayout(duelBox);
    m_duels = new QComboBox(duelBox);
    for (DuelMode mode : {DuelMode::Timing, DuelMode::Rush, DuelMode::None})
        m_duels->addItem(toString(mode), static_cast<int>(mode));
    m_duels->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_duels->setMinimumWidth(150);
    duelLayout->addWidget(m_duels);
    m_duelHint = new QLabel(duelBox);
    m_duelHint->setObjectName(QStringLiteral("hint"));
    m_duelHint->setWordWrap(true);
    duelLayout->addWidget(m_duelHint, 1);
    root->addWidget(duelBox);
    connect(m_duels, &QComboBox::activated, this, [this] {
        const auto mode = static_cast<DuelMode>(m_duels->currentData().toInt());
        QSettings().setValue(QStringLiteral("duelMode"), static_cast<int>(mode));
        MatchRules::setDefaultDuelMode(mode);
        refresh();
    });

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
    back->setProperty("variant", QStringLiteral("ghost"));
    back->setProperty("glyph", QStringLiteral("back"));
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

void SettingsPage::loadPreferences()
{
    const int mode = QSettings().value(QStringLiteral("duelMode"), 0).toInt();
    MatchRules::setDefaultDuelMode(static_cast<DuelMode>(std::clamp(mode, 0, 2)));
}

QString SettingsPage::duelDescription(DuelMode mode)
{
    const QString dash = Controls::instance().text(Action::Dash);
    switch (mode) {
    case DuelMode::Timing:
        return tr("Appuyez sur %1 quand le curseur traverse la zone verte, de plus en plus étroite : "
                  "un raté perd le duel.").arg(dash);
    case DuelMode::Rush:
        return tr("Tir à la corde : appuyez le plus vite possible sur %1 pour pousser le curseur vers "
                  "l'adversaire. Au bout de 3,5 s, le plus avancé gagne ; pile au milieu, match nul.")
            .arg(dash);
    case DuelMode::None:
        break;
    }
    return tr("Pas de duel : deux toupies qui se percutent en plein dash se repoussent comme lors "
              "d'un choc ordinaire.");
}

void SettingsPage::refresh()
{
    m_status->clear();
    const MatchRules defaults;
    m_duels->setCurrentIndex(m_duels->findData(static_cast<int>(defaults.duelMode)));
    m_duelHint->setText(duelDescription(defaults.duelMode) + QLatin1Char(' ')
                        + tr("C'est le duel par défaut des combats, des parties en réseau et des tournois ; la "
                           "personnalisation des règles permet d'en choisir un autre."));
    const Controls &controls = Controls::instance();
    for (int a = 0; a < kActionCount; ++a) {
        for (int slot = 0; slot < Controls::kSlots; ++slot)
            m_buttons[a][slot]->setText(
                Controls::codeText(controls.binding(static_cast<Action>(a), slot)));
    }
}

} // namespace toupie
