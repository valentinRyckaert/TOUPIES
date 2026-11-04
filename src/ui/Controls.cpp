#include "ui/Controls.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QSettings>
#include <QStringList>

namespace toupie {

namespace {
const std::array<std::array<int, Controls::kSlots>, kActionCount> kDefaults{{
    {Qt::Key_Left, Qt::Key_Q},
    {Qt::Key_Right, Qt::Key_D},
    {Qt::Key_Up, Qt::Key_Z},
    {Qt::Key_Down, Qt::Key_S},
    {Qt::Key_Space, Controls::mouseCode(Qt::LeftButton)},
    {Qt::Key_E, Qt::Key_Shift},
    {Qt::Key_Return, Qt::Key_Enter},
    {Qt::Key_R, 0},
}};

QString settingsKey(int action)
{
    return QStringLiteral("controls/action%1").arg(action);
}
} // namespace

Controls &Controls::instance()
{
    static Controls controls;
    return controls;
}

Controls::Controls()
    : m_codes(kDefaults)
{
    const QSettings settings;
    for (int a = 0; a < kActionCount; ++a) {
        const QStringList saved = settings.value(settingsKey(a)).toString().split(QLatin1Char(','));
        if (saved.size() != kSlots)
            continue;
        for (int slot = 0; slot < kSlots; ++slot)
            m_codes[a][slot] = saved.at(slot).toInt() == Qt::Key_Escape ? 0 : saved.at(slot).toInt();
    }
}

QList<Action> Controls::actionsFor(int code) const
{
    QList<Action> actions;
    for (int a = 0; a < kActionCount && code != 0; ++a) {
        if (m_codes[a][0] == code || m_codes[a][1] == code)
            actions.append(static_cast<Action>(a));
    }
    return actions;
}

int Controls::binding(Action action, int slot) const
{
    return m_codes[static_cast<int>(action)][slot];
}

std::optional<Action> Controls::bind(Action action, int slot, int code)
{
    if (code == Qt::Key_Escape)
        return std::nullopt;
    std::optional<Action> previous;
    for (int a = 0; a < kActionCount && code != 0; ++a) {
        for (int &other : m_codes[a]) {
            if (other == code && !(a == static_cast<int>(action))) {
                other = 0;
                previous = static_cast<Action>(a);
            }
        }
    }
    auto &codes = m_codes[static_cast<int>(action)];
    if (codes[1 - slot] == code)
        codes[1 - slot] = 0; // Déjà sur l'autre case de la même action.
    codes[slot] = code;
    save();
    return previous;
}

void Controls::reset()
{
    m_codes = kDefaults;
    save();
}

void Controls::save() const
{
    QSettings settings;
    for (int a = 0; a < kActionCount; ++a)
        settings.setValue(settingsKey(a),
                          QStringLiteral("%1,%2").arg(m_codes[a][0]).arg(m_codes[a][1]));
}

QString Controls::actionName(Action action)
{
    switch (action) {
    case Action::SteerLeft:
        return QCoreApplication::translate("Controls", "Gauche (pilotage, direction)");
    case Action::SteerRight:
        return QCoreApplication::translate("Controls", "Droite (pilotage, direction)");
    case Action::SteerUp:
        return QCoreApplication::translate("Controls", "Haut (pilotage, force +)");
    case Action::SteerDown:
        return QCoreApplication::translate("Controls", "Bas (pilotage, force −)");
    case Action::Dash:
        return QCoreApplication::translate("Controls", "Dash / appui en duel");
    case Action::Special:
        return QCoreApplication::translate("Controls", "Coup spécial");
    case Action::Launch:
        return QCoreApplication::translate("Controls", "Lancer la toupie");
    case Action::Rematch:
        return QCoreApplication::translate("Controls", "Revanche / retour au lobby");
    }
    return {};
}

QString Controls::codeText(int code)
{
    auto tr = [](const char *text) { return QCoreApplication::translate("Controls", text); };
    switch (code) {
    case 0:
        return tr("—");
    case Controls::mouseCode(Qt::LeftButton):
        return tr("Clic gauche");
    case Controls::mouseCode(Qt::RightButton):
        return tr("Clic droit");
    case Controls::mouseCode(Qt::MiddleButton):
        return tr("Clic molette");
    case Controls::mouseCode(Qt::BackButton):
        return tr("Bouton précédent");
    case Controls::mouseCode(Qt::ForwardButton):
        return tr("Bouton suivant");
    case Qt::Key_Space:
        return tr("Espace");
    case Qt::Key_Return:
        return tr("Entrée");
    case Qt::Key_Enter:
        return tr("Entrée (pavé num.)");
    case Qt::Key_Shift:
        return tr("Maj");
    case Qt::Key_Control:
        return tr("Ctrl");
    case Qt::Key_Alt:
        return tr("Alt");
    case Qt::Key_Left:
        return QStringLiteral("←");
    case Qt::Key_Right:
        return QStringLiteral("→");
    case Qt::Key_Up:
        return QStringLiteral("↑");
    case Qt::Key_Down:
        return QStringLiteral("↓");
    default:
        break;
    }
    if (code < 0)
        return tr("Bouton souris %1").arg(-code);
    return QKeySequence(code).toString(QKeySequence::NativeText);
}

QString Controls::text(Action action) const
{
    QStringList parts;
    for (int code : m_codes[static_cast<int>(action)]) {
        if (code != 0)
            parts << codeText(code);
    }
    return parts.isEmpty() ? QCoreApplication::translate("Controls", "aucune touche")
                           : parts.join(QStringLiteral(" / "));
}

QString Controls::helpHtml() const
{
    auto tr = [](const char *text) { return QCoreApplication::translate("Controls", text); };
    auto key = [this](Action a) { return QStringLiteral("<b>%1</b>").arg(text(a).toHtmlEscaped()); };
    auto row = [](const QString &what, const QString &how) {
        return QStringLiteral("<tr><td style='padding:3px 14px 3px 0'>%1</td><td>%2</td></tr>")
            .arg(what, how);
    };
    auto section = [](const QString &title) {
        return QStringLiteral("<tr><td colspan='2' style='padding-top:10px; color:#8ee05a'>"
                              "<b>%1</b></td></tr>")
            .arg(title);
    };
    QString html = QStringLiteral("<table>");
    html += section(tr("Visée"));
    html += row(tr("Direction"), tr("souris, ou %1 et %2").arg(key(Action::SteerLeft),
                                                            key(Action::SteerRight)));
    html += row(tr("Force"), tr("distance du curseur (flèche plus ou moins longue), ou %1 et %2")
                                 .arg(key(Action::SteerUp), key(Action::SteerDown)));
    html += row(tr("Lancer"), tr("%1 ou %2").arg(key(Action::Launch), key(Action::Dash)));
    html += section(tr("Combat"));
    html += row(tr("Pilotage (touches maintenues)"),
                QStringLiteral("%1 · %2 · %3 · %4")
                    .arg(key(Action::SteerLeft), key(Action::SteerRight), key(Action::SteerUp),
                         key(Action::SteerDown)));
    html += row(tr("Dash"), key(Action::Dash));
    html += row(tr("Coup spécial"),
                tr("%1 — une fois par partie, coûte un quart de l'endurance du moment")
                    .arg(key(Action::Special)));
    html += row(tr("Duel"), tr("%1 quand le curseur est dans la zone verte").arg(key(Action::Dash)));
    html += section(tr("Fin de combat et menus"));
    html += row(tr("Revanche (solo) / retour au lobby (hôte)"), key(Action::Rematch));
    html += row(tr("Retour au menu / quitter le lobby / abandonner"), tr("<b>Échap</b>"));
    html += QStringLiteral("</table>");
    return html;
}

} // namespace toupie
