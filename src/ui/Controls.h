#pragma once

#include <QList>
#include <QString>

#include <array>
#include <optional>

namespace toupie {

/// Actions du joueur que l'on peut associer à des touches ou à des boutons
/// de la souris. Pendant la visée, les touches de pilotage règlent la
/// direction (gauche / droite) et la force (haut / bas) du lancer, et le dash
/// lance la toupie ; en duel, le dash sert d'appui.
enum class Action { SteerLeft, SteerRight, SteerUp, SteerDown, Dash, Special, Launch, Rematch };
constexpr int kActionCount = 8;

/// Associations touches / souris -> actions, enregistrées dans les réglages.
///
/// Une entrée est un code : > 0 pour une touche (Qt::Key), < 0 pour un bouton
/// de souris (-Qt::MouseButton), 0 pour aucune. Chaque action a deux entrées
/// et une entrée ne sert qu'à une action. Échap reste réservée (quitter).
class Controls
{
public:
    static constexpr int kSlots = 2;

    /// Associations du jeu (chargées des réglages au premier appel).
    static Controls &instance();
    /// Associations lues dans les réglages (ou par défaut).
    Controls();

    static constexpr int mouseCode(int button) { return -button; }

    QList<Action> actionsFor(int code) const;
    bool triggers(int code, Action action) const { return actionsFor(code).contains(action); }
    int binding(Action action, int slot) const;
    /// Associe `code` (0 : efface) ; l'entrée est retirée de l'action qui
    /// l'utilisait, renvoyée le cas échéant. Enregistré aussitôt.
    std::optional<Action> bind(Action action, int slot, int code);
    void reset();

    static QString actionName(Action action);
    static QString codeText(int code);
    /// Entrées d'une action (« Espace / Clic gauche »).
    QString text(Action action) const;
    /// Récapitulatif des commandes (HTML), d'après les associations actuelles.
    QString helpHtml() const;

private:
    void save() const;

    std::array<std::array<int, kSlots>, kActionCount> m_codes{};
};

} // namespace toupie
