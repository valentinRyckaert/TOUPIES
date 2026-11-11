#pragma once

#include "game/Tournament.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QPushButton;
class QFormLayout;
class QLabel;
class QStackedWidget;

namespace toupie {

class RulesWidget;
class TopRepository;
class TournamentSettingsWidget;

/// Section « Tournois », en étapes : choix solo / réseau local, puis
/// réglages du tournoi solo (la personnalisation s'ouvre dans une fenêtre).
class TournamentSetupPage : public QWidget
{
    Q_OBJECT

public:
    explicit TournamentSetupPage(TopRepository &repository, QWidget *parent = nullptr);

    /// Recharge les toupies disponibles et revient au choix du mode.
    void refresh();
    TournamentSettings settings() const;

signals:
    void backRequested();
    /// Tournoi solo : réglages et toupies du joueur.
    void soloRequested(const toupie::TournamentSettings &settings, const QList<toupie::Top> &tops);
    /// Tournoi en réseau : créer ou rejoindre un lobby de tournoi.
    void lanRequested();
    /// Reprendre le tournoi solo sauvegardé.
    void resumeRequested();

private:
    void updateTops();

    TopRepository &m_repository;
    QList<Top> m_tops;
    QStackedWidget *m_steps = nullptr;
    TournamentSettingsWidget *m_settings = nullptr;
    QFormLayout *m_soloForm = nullptr;
    QComboBox *m_stadium = nullptr;
    RulesWidget *m_rules = nullptr;
    QLabel *m_rulesSummary = nullptr;
    QList<QComboBox *> m_topCombos;
    QPushButton *m_resume = nullptr;
};

} // namespace toupie
