#pragma once

#include "core/Top.h"

#include <QList>
#include <QString>

namespace toupie {

class PartCatalog;

/// Stockage des toupies assemblées par le joueur (fichier JSON).
class TopRepository
{
public:
    /// @param filePath chemin du fichier JSON ; vide = emplacement par défaut
    ///        dans le dossier de données de l'application.
    explicit TopRepository(const PartCatalog &catalog, QString filePath = {});

    static QString defaultFilePath();
    const QString &filePath() const { return m_filePath; }

    /// Recharge le fichier. Retourne false si le fichier existe mais est illisible.
    bool load();
    bool save() const;

    const QList<Top> &customTops() const { return m_tops; }
    /// Toupies pré-assemblées puis toupies du joueur.
    QList<Top> allTops() const;

    /// Ajoute la toupie, ou remplace celle portant le même nom.
    void upsert(const Top &top);
    bool remove(const QString &name);
    bool contains(const QString &name) const;

private:
    int indexOf(const QString &name) const;

    const PartCatalog &m_catalog;
    QString m_filePath;
    QList<Top> m_tops;
};

} // namespace toupie
