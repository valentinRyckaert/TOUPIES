#pragma once

#include "session/MatchSession.h"

#include <QWidget>

#include <memory>

class QLabel;
class QPushButton;

namespace toupie {

class ArenaWidget;

/// Écran de combat : l'arène et les actions disponibles autour.
class BattlePage : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Solo, LanHost, LanClient };

    explicit BattlePage(QWidget *parent = nullptr);
    ~BattlePage() override;

    /// Affiche un nouveau combat ; la page devient propriétaire de la session.
    void start(std::unique_ptr<MatchSession> session, Mode mode);
    /// Arrête et détruit la session en cours.
    void stop();

    MatchSession *session() const { return m_session.get(); }
    Mode mode() const { return m_mode; }

signals:
    /// Solo : retour au menu ; réseau : quitter le lobby.
    void leaveRequested();
    /// Solo : changer de toupies.
    void setupRequested();
    /// Solo : revanche ; hôte : retour au lobby.
    void rematchRequested();

private:
    void updateButtons();
    void onFinished(int winner);

    ArenaWidget *m_arena = nullptr;
    QLabel *m_score = nullptr;
    QPushButton *m_leaveButton = nullptr;
    QPushButton *m_setupButton = nullptr;
    QPushButton *m_rematchButton = nullptr;
    QPushButton *m_specialButton = nullptr;
    std::unique_ptr<MatchSession> m_session;
    Mode m_mode = Mode::Solo;
    int m_wins = 0;
    int m_losses = 0;
    int m_draws = 0;
};

} // namespace toupie
