#pragma once

#include "session/MatchSession.h"
#include "ui/Controls.h"

#include <memory>

namespace toupie {

/// Tutoriel : un combat scénarisé contre une toupie d'entraînement dont la
/// trajectoire est prévue à l'avance (lancer fixe, puis approche lente vers
/// le joueur). À chaque étape, une bulle met le jeu en pause et présente une
/// action ; seule la touche présentée reprend la partie (en l'exécutant).
/// Le duel est guidé : un raté du joueur n'est pas compté (une bulle explique
/// l'erreur), il gagne toujours et la toupie d'entraînement est éjectée.
class TutorialSession : public MatchSession
{
    Q_OBJECT

public:
    enum class Step {
        Aim, Power, Launch, Steer, Dash, Special,
        Duel,      ///< Attente du moment où les deux toupies peuvent dasher.
        AfterDuel, ///< Les deux dashs sont partis.
        DuelIntro, DuelPlay, DuelMiss,
        Ejection,  ///< Duel gagné : la toupie d'entraînement sort du stadium.
        Done
    };

    explicit TutorialSession(QObject *parent = nullptr);
    ~TutorialSession() override;

    Step step() const { return m_step; }
    /// Jeu figé en attendant la touche présentée.
    bool isPaused() const { return m_paused; }
    /// Texte de la bulle (vide : aucune) et toupie qu'elle désigne (-1 : aucune).
    QString bubble() const;
    int bubbleTarget() const;
    /// Filtre les actions du joueur : false si elle est ignorée à cette étape ;
    /// l'action présentée par la bulle fait avancer le tutoriel.
    bool handleAction(Action action);

    void update(double dt) override;
    void submitAim(double angle, double power) override;
    void requestDash() override;
    void requestSpecial() override;
    void setSteering(Vec2 direction) override;

private:
    void pause() { m_paused = true; }
    void next(Step step);
    double distance() const;

    std::unique_ptr<Battle> m_battle;
    int m_duelHits = 0;
    Step m_step = Step::Aim;
    bool m_paused = true;
    double m_stepTimer = 0.0;
    double m_angles[2] = {0.0, 0.0};
    double m_powers[2] = {1.0, 0.7};
};

} // namespace toupie
