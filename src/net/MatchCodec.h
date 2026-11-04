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
    MatchRules rules;
    quint32 seed = 0;
    QList<int> playerIds;     ///< -1 : place tenue par un bot (simulé par l'hôte).
    QStringList names;
    QList<Top> tops;
    QList<int> botLevels;     ///< Difficulty de chaque place, -1 pour un humain.
    int match = -1;           ///< Tournoi : indice du match dans le tour.
    std::optional<MatchSeries> series;
};

std::optional<MatchSetup> parseStart(const QJsonObject &start, const PartCatalog &catalog);
QJsonObject encodeStart(const MatchSetup &setup);

/// Encode l'état dynamique d'un combat (phase, positions, endurance,
/// résultat) et les chocs récents dans un message « state ».
QJsonObject encodeState(const MatchState &state, const QList<ImpactEvent> &impacts);

/// Applique un message « state » à un état existant (dont les toupies,
/// noms et stadium ont été fixés au lancement). Retourne false si le
/// message est incohérent avec l'état (nombre de joueurs différent).
bool decodeState(const QJsonObject &message, MatchState &state, QList<ImpactEvent> *impacts);

} // namespace toupie::net
