#pragma once

#include "session/MatchSession.h"

#include <QJsonObject>
#include <QList>
#include <QStringList>

#include <optional>

namespace toupie {
class PartCatalog;
}

namespace toupie::net {

/// Contenu d'un message « start » : participants (dans l'ordre des places)
/// et paramètres du combat.
struct MatchSetup
{
    Stadium stadium;
    quint32 seed = 0;
    QList<int> playerIds;
    QStringList names;
    QList<Top> tops;
};

std::optional<MatchSetup> parseStart(const QJsonObject &start, const PartCatalog &catalog);

/// Encode l'état dynamique d'un combat (phase, positions, endurance,
/// résultat) et les chocs récents dans un message « state ».
QJsonObject encodeState(const MatchState &state, const QList<ImpactEvent> &impacts);

/// Applique un message « state » à un état existant (dont les toupies,
/// noms et stadium ont été fixés au lancement). Retourne false si le
/// message est incohérent avec l'état (nombre de joueurs différent).
bool decodeState(const QJsonObject &message, MatchState &state, QList<ImpactEvent> *impacts);

} // namespace toupie::net
