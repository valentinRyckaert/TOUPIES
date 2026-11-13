#pragma once

#include "game/BattleConfig.h"

#include <QWidget>

#include <array>

class QComboBox;
class QLabel;
class QPushButton;

namespace toupie {

/// Paramètres : langue du jeu, son (musiques, effets), touches du clavier et boutons de la souris
/// associés à chaque action (cliquer sur une case puis appuyer sur la touche voulue).
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    /// Réaffiche les associations actuelles.
    void refresh();

    /// Applique les préférences enregistrées (type de duel) ; à appeler au démarrage.
    static void loadPreferences();
    /// Règle d'un type de duel, avec la touche de dash actuelle.
    static QString duelDescription(DuelMode mode);

signals:
    void backRequested();
    /// La langue a changé : le jeu doit redémarrer pour l'appliquer.
    void restartRequested();

private:
    std::array<std::array<QPushButton *, 2>, 8> m_buttons{};
    QLabel *m_status = nullptr;
    QComboBox *m_language = nullptr;
    QComboBox *m_duels = nullptr;
    QLabel *m_duelHint = nullptr;
};

} // namespace toupie
