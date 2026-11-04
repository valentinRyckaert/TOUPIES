#include "ui/TutorialSession.h"

#include "core/PartCatalog.h"

#include <cmath>

namespace toupie {

namespace {
constexpr std::uint32_t kSeed = 7;  ///< Combat identique à chaque essai.
constexpr double kApproach = 0.6;   ///< Pilotage de la toupie d'entraînement.
constexpr int kDuelHits = 3;        ///< Réussites du joueur avant le raté adverse.
constexpr double kEjection = 3.0;   ///< Vitesse imprimée au perdant du duel.

bool isSteering(Action a)
{
    return a == Action::SteerLeft || a == Action::SteerRight || a == Action::SteerUp
        || a == Action::SteerDown;
}

bool validates(Action a)
{
    return a == Action::Dash || a == Action::Launch; // Clic gauche par défaut.
}
} // namespace

TutorialSession::TutorialSession(QObject *parent)
    : MatchSession(parent)
{
    const QList<Top> presets = PartCatalog::instance().presets();
    BattleConfig config;
    config.duelTimeout = 1e9; // Le joueur prend son temps : pas de défaite par inactivité.
    m_battle = std::make_unique<Battle>(QList<Top>{presets.at(0), presets.at(1)},
                                        Stadium::classic(), config, kSeed);
    initFromBattle(*m_battle, {tr("Vous"), tr("Toupie d'entraînement")});
    // La toupie d'entraînement part toujours droit vers le centre, doucement.
    m_angles[1] = (-m_battle->startPosition(1)).angle();
    m_state.aimed[1] = true;
}

TutorialSession::~TutorialSession() = default;

void TutorialSession::next(Step step)
{
    m_step = step;
    m_paused = false;
    m_stepTimer = 0.0;
}

double TutorialSession::distance() const
{
    return (m_battle->spinner(1).position - m_battle->spinner(0).position).length();
}

QString TutorialSession::bubble() const
{
    const Controls &c = Controls::instance();
    auto key = [&c](Action a) { return QStringLiteral("« %1 »").arg(c.text(a)); };
    switch (m_step) {
    case Step::Aim:
        return tr("Bienvenue dans le tutoriel !\nVotre toupie est à gauche. Visez avec la souris "
                  "puis cliquez, ou visez avec %1 et %2.")
            .arg(key(Action::SteerLeft), key(Action::SteerRight));
    case Step::Power:
        return tr("La longueur de la flèche donne la force du lancer : éloignez ou rapprochez "
                  "le curseur puis cliquez, ou appuyez sur %1 / %2.")
            .arg(key(Action::SteerUp), key(Action::SteerDown));
    case Step::Launch:
        return tr("Parfait. Lancez votre toupie avec %1 (ou %2).")
            .arg(key(Action::Dash), key(Action::Launch));
    case Step::Done:
        return tr("Bravo ! Duel gagné et toupie adverse éjectée : vous connaissez toutes les "
                  "actions. Appuyez sur Échap pour revenir au menu.");
    default:
        break;
    }
    if (!m_paused)
        return {};
    switch (m_step) {
    case Step::Steer:
        return tr("Votre toupie avance seule, dans le sens inverse de sa rotation, et revient "
                  "vers le centre. Maintenez %1, %2, %3 ou %4 pour la piloter légèrement.")
            .arg(key(Action::SteerLeft), key(Action::SteerRight), key(Action::SteerUp),
                 key(Action::SteerDown));
    case Step::Dash:
        return tr("L'adversaire est proche : appuyez sur %1 pour dasher. Le dash fonce sur "
                  "lui avec plus d'attaque, contre un peu d'endurance.")
            .arg(key(Action::Dash));
    case Step::Special:
        return tr("Votre coup spécial (donné par le boulon) se lance avec %1. Une seule fois "
                  "par partie : il coûte un quart de l'endurance du moment, et son effet "
                  "compte autant que ses dégâts : lancez-le au bon moment. Essayez-le !")
            .arg(key(Action::Special));
    case Step::Duel:
        return tr("Il fonce sur vous ! Répondez par un dash avec %1 : quand deux toupies se "
                  "percutent en plein dash, elles s'affrontent en duel.")
            .arg(key(Action::Dash));
    case Step::DuelIntro:
        return tr("Duel ! Les toupies restent bloquées l'une contre l'autre. En bas de l'écran, "
                  "un curseur blanc parcourt votre barre : appuyez sur %1 quand il est dans la "
                  "zone verte. La zone rétrécit à chaque réussite ; le premier qui rate subit "
                  "1,5× les dégâts du dash adverse.\nAppuyez sur %1 pour commencer.")
            .arg(key(Action::Dash));
    case Step::DuelMiss:
        return tr("Raté : le curseur était dans le rouge. Attendez qu'il passe au centre, dans "
                  "la zone verte, avant d'appuyer. Pour cette fois, l'erreur ne compte pas !\n"
                  "Appuyez sur %1 pour reprendre.")
            .arg(key(Action::Dash));
    default:
        return {};
    }
}

int TutorialSession::bubbleTarget() const
{
    switch (m_step) {
    case Step::Dash:
    case Step::Special:
    case Step::Duel:
        return 1;
    case Step::Done:
        return -1;
    default:
        return 0;
    }
}

bool TutorialSession::handleAction(Action action)
{
    if (action == Action::Rematch)
        return true;
    switch (m_step) {
    case Step::Aim:
        // Valider la visée (clic) ou viser au clavier : étape suivante, sans lancer.
        if (action == Action::SteerLeft || action == Action::SteerRight) {
            next(Step::Power);
            return true;
        }
        if (validates(action))
            next(Step::Power);
        return false;
    case Step::Power:
        if (action == Action::SteerUp || action == Action::SteerDown || validates(action))
            next(Step::Launch);
        return isSteering(action);
    case Step::Launch:
        return isSteering(action) || validates(action);
    case Step::Done:
        return false;
    default:
        break;
    }

    if (!m_paused) {
        if (m_step == Step::DuelPlay && action == Action::Dash) {
            // Duel guidé : un appui hors de la zone verte n'est pas compté.
            const DuelSide &side = m_state.duel.sides[std::max(0, m_state.duel.sideOf(0))];
            if (std::abs(side.cursor) <= side.zone)
                return true;
            m_step = Step::DuelMiss;
            pause();
            return false;
        }
        // Entre deux bulles : pilotage libre ; le dash attend son tour.
        return isSteering(action) || (action == Action::Dash && m_step == Step::Special);
    }
    switch (m_step) {
    case Step::Steer:
        if (!isSteering(action))
            return false;
        next(Step::Dash);
        return true;
    case Step::Dash:
        if (action != Action::Dash)
            return false;
        next(Step::Special);
        return true;
    case Step::Special:
        if (action != Action::Special)
            return false;
        next(Step::Duel);
        return true;
    case Step::Duel:
        if (action != Action::Dash)
            return false;
        m_battle->requestDash(1, 0); // Le dash du joueur part au même instant.
        next(Step::AfterDuel);
        return true;
    case Step::DuelIntro:
    case Step::DuelMiss:
        // Reprise du duel : cet appui ne compte pas comme un appui de duel.
        if (action == Action::Dash)
            next(Step::DuelPlay);
        return false;
    default:
        return false;
    }
}

void TutorialSession::submitAim(double angle, double power)
{
    if (m_step != Step::Launch || m_state.phase != MatchPhase::Aiming)
        return;
    m_angles[0] = angle;
    m_powers[0] = power;
    m_state.aimed[0] = true;
    m_state.countdown = kCountdownSeconds;
    next(Step::Steer);
    setPhase(MatchPhase::Countdown);
}

void TutorialSession::requestDash()
{
    if (m_state.phase != MatchPhase::Fighting || m_paused)
        return;
    m_battle->requestDash(0);
    if (m_step == Step::DuelPlay && ++m_duelHits >= kDuelHits && m_battle->duel().active) {
        // Le joueur gagne toujours : la toupie d'entraînement rate, puis est éjectée.
        m_battle->duelPress(1, 1.0);
        const Vec2 away = (m_battle->spinner(1).position - m_battle->spinner(0).position).normalized();
        m_battle->applyImpulse(1, away * kEjection);
        next(Step::Ejection);
    }
}

void TutorialSession::requestSpecial()
{
    if (m_state.phase == MatchPhase::Fighting && !m_paused)
        m_battle->requestSpecial(0);
}

void TutorialSession::setSteering(Vec2 direction)
{
    m_battle->setSteering(0, direction);
}

void TutorialSession::update(double dt)
{
    switch (m_state.phase) {
    case MatchPhase::Aiming:
    case MatchPhase::Finished:
        return;
    case MatchPhase::Countdown:
        m_state.countdown -= dt;
        if (m_state.countdown <= 0.0) {
            m_state.countdown = 0.0;
            m_battle->launch({m_angles[0], m_angles[1]}, {m_powers[0], m_powers[1]});
            syncFromBattle(*m_battle);
            setPhase(MatchPhase::Fighting);
        }
        return;
    case MatchPhase::Fighting:
        break;
    }
    if (m_paused)
        return; // Bulle affichée : le jeu attend la touche présentée.

    m_stepTimer += dt;
    switch (m_step) {
    case Step::Steer:
        if (m_stepTimer > 1.0)
            pause();
        break;
    case Step::Dash:
        if (m_stepTimer > 1.5 && m_battle->canDash(0) && (distance() < 0.55 || m_stepTimer > 8.0))
            pause();
        break;
    case Step::Special:
        if (m_stepTimer > 1.5 && m_battle->canSpecial(0))
            pause();
        break;
    case Step::Duel:
        if (m_battle->canDash(0) && m_battle->canDash(1) && distance() < 0.7)
            pause();
        break;
    case Step::AfterDuel:
        if (m_battle->duel().active) {
            m_step = Step::DuelIntro;
            pause();
            syncFromBattle(*m_battle); // Barres de duel visibles pendant l'explication.
            return;
        }
        if (m_stepTimer > 2.0)
            next(Step::Duel); // Dashs manqués : on recommence.
        break;
    default:
        break;
    }
    if (m_paused)
        return;

    if (m_step < Step::AfterDuel) {
        // Trajectoire prévue : la toupie d'entraînement se rapproche lentement du joueur.
        const Vec2 toward = m_battle->spinner(0).position - m_battle->spinner(1).position;
        m_battle->setSteering(1, toward.normalized() * kApproach);
    } else {
        m_battle->setSteering(1, {});
    }
    m_battle->advance(dt);
    syncFromBattle(*m_battle);
    if (m_battle->isFinished()) {
        m_step = Step::Done;
        setPhase(MatchPhase::Finished);
    }
}

} // namespace toupie
