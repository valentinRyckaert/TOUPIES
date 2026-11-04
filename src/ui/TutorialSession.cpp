#include "ui/TutorialSession.h"

#include "core/PartCatalog.h"

namespace toupie {

namespace {
constexpr std::uint32_t kSeed = 7;          ///< Combat identique à chaque essai.
constexpr double kApproach = 0.6;           ///< Pilotage de la toupie d'entraînement.

bool isSteering(Action a)
{
    return a == Action::SteerLeft || a == Action::SteerRight || a == Action::SteerUp
        || a == Action::SteerDown;
}
} // namespace

TutorialSession::TutorialSession(QObject *parent)
    : MatchSession(parent)
{
    const QList<Top> presets = PartCatalog::instance().presets();
    m_battle = std::make_unique<Battle>(QList<Top>{presets.at(0), presets.at(1)},
                                        Stadium::classic(), BattleConfig{}, kSeed);
    initFromBattle(*m_battle, {tr("Vous"), tr("Toupie d'entraînement")});
    // La toupie d'entraînement part toujours droit vers le centre, doucement.
    m_angles[1] = (-m_battle->startPosition(1)).angle();
    m_state.aimed[1] = true;
    m_bot = std::make_unique<BotController>(1, Difficulty::Easy, kSeed);
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
                  "ou avec %1 et %2 : essayez maintenant.")
            .arg(key(Action::SteerLeft), key(Action::SteerRight));
    case Step::Power:
        return tr("La longueur de la flèche donne la force du lancer : éloignez le curseur ou "
                  "appuyez sur %1 / %2.")
            .arg(key(Action::SteerUp), key(Action::SteerDown));
    case Step::Launch:
        return tr("Parfait. Lancez votre toupie avec %1 (ou %2).")
            .arg(key(Action::Launch), key(Action::Dash));
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
                  "par partie : il coûte la moitié de l'endurance du moment, alors gardez-le "
                  "pour un moment décisif. Essayez-le !")
            .arg(key(Action::Special));
    case Step::Duel:
        return tr("Il fonce sur vous ! Répondez par un dash (%1) : deux dashs qui se percutent "
                  "déclenchent un duel. Appuyez ensuite sur %1 quand le curseur est dans la "
                  "zone verte ; le premier qui rate subit 1,5× les dégâts du dash adverse.")
            .arg(key(Action::Dash));
    case Step::Done:
        return tr("Bravo, vous connaissez toutes les actions !\nLa toupie d'entraînement va "
                  "maintenant se défendre : appuyez sur %1 pour finir le combat librement, ou "
                  "sur Échap pour quitter.")
            .arg(key(Action::Launch));
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
    case Step::Free:
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
        if (action != Action::SteerLeft && action != Action::SteerRight)
            return false;
        next(Step::Power);
        return true;
    case Step::Power:
        if (action == Action::SteerUp || action == Action::SteerDown)
            next(Step::Launch);
        return isSteering(action);
    case Step::Launch:
        return isSteering(action) || action == Action::Launch || action == Action::Dash;
    case Step::Free:
        return true;
    default:
        break;
    }

    if (!m_paused) {
        // Entre deux bulles : pilotage libre, chaque action attend son tour.
        return isSteering(action)
            || (action == Action::Dash && m_step >= Step::Special && m_step != Step::Duel);
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
    case Step::Done:
        if (action != Action::Launch)
            return false;
        next(Step::Free);
        return true;
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
    if (m_state.phase == MatchPhase::Fighting && !m_paused)
        m_battle->requestDash(0);
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
    const bool duel = m_battle->duel().active;
    m_duelStarted = m_duelStarted || duel;
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
        // Fin du duel (ou dashs qui se sont manqués) : dernière bulle.
        if ((m_duelStarted && !duel && m_stepTimer > 2.0) || (!m_duelStarted && m_stepTimer > 4.0)) {
            m_step = Step::Done;
            pause();
        }
        break;
    default:
        break;
    }
    if (m_paused)
        return;

    if (m_step == Step::Free || duel) {
        m_bot->update(*m_battle, dt);
    } else {
        // Trajectoire prévue : la toupie d'entraînement se rapproche lentement du joueur.
        const Vec2 toward = m_battle->spinner(0).position - m_battle->spinner(1).position;
        m_battle->setSteering(1, toward.normalized() * kApproach);
    }
    m_battle->advance(dt);
    syncFromBattle(*m_battle);
    if (m_battle->isFinished())
        setPhase(MatchPhase::Finished);
}

} // namespace toupie
