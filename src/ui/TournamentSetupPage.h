#pragma once

#include "game/Tournament.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QGroupBox;
class QPushButton;
class QRadioButton;

namespace toupie {

class RulesWidget;
class TopRepository;
class TournamentSettingsWidget;

/// Section « Tournois » : tournoi solo contre des bots ou en réseau local.
class TournamentSetupPage : public QWidget
{
    Q_OBJECT

public:
    explicit TournamentSetupPage(TopRepository &repository, QWidget *parent = nullptr);

    /// Recharge les toupies disponibles.
    void refresh();
    TournamentSettings settings() const;

signals:
    void backRequested();
    /// Tournoi solo : réglages et toupies du joueur.
    void soloRequested(const toupie::TournamentSettings &settings, const QList<toupie::Top> &tops);
    /// Tournoi en réseau : créer ou rejoindre un lobby de tournoi.
    void lanRequested();

private:
    void updateMode();

    TopRepository &m_repository;
    QList<Top> m_tops;
    QRadioButton *m_solo = nullptr;
    QRadioButton *m_lan = nullptr;
    TournamentSettingsWidget *m_settings = nullptr;
    QGroupBox *m_soloBox = nullptr;
    QComboBox *m_stadium = nullptr;
    RulesWidget *m_rules = nullptr;
    QList<QComboBox *> m_topCombos;
    QPushButton *m_start = nullptr;
};

} // namespace toupie
