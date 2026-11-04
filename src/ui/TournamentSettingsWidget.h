#pragma once

#include "game/Tournament.h"

#include <QGroupBox>

class QComboBox;
class QDialog;
class QLabel;
class QPushButton;
class QTableWidget;

namespace toupie {

/// Réglages d'un tournoi : format (ligue / élimination directe), type de
/// match (express / classique), nombre de participants (4, 8, 16 ou 32 en
/// élimination directe) et niveau de chaque bot (dans une fenêtre
/// surgissante). En lecture seule pour les clients d'un lobby.
class TournamentSettingsWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit TournamentSettingsWidget(QWidget *parent = nullptr);

    /// Réglages affichés ; le stadium et la personnalisation sont conservés
    /// tels que passés à setSettings().
    TournamentSettings settings() const { return m_settings; }
    void setSettings(const TournamentSettings &settings);
    /// Nombre de joueurs humains (les autres participants sont des bots).
    void setHumans(int humans);
    void setReadOnly(bool readOnly);

signals:
    void settingsChanged(const toupie::TournamentSettings &settings);

private:
    void refresh();
    void edited();

    TournamentSettings m_settings;
    int m_humans = 1;
    bool m_readOnly = false;
    bool m_updating = false;
    QComboBox *m_format = nullptr;
    QComboBox *m_match = nullptr;
    QComboBox *m_participants = nullptr;
    QLabel *m_matchHelp = nullptr;
    QPushButton *m_botsButton = nullptr;
    QComboBox *m_allBots = nullptr;
    QTableWidget *m_bots = nullptr;
    QDialog *m_botsPopup = nullptr;
};

} // namespace toupie
