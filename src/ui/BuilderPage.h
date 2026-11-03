#pragma once

#include "core/Top.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace toupie {

class PartCatalog;
class StatsWidget;
class TopRepository;

/// Atelier d'assemblage : choix des trois pièces, aperçu des stats et
/// gestion des toupies sauvegardées.
class BuilderPage : public QWidget
{
    Q_OBJECT

public:
    BuilderPage(const PartCatalog &catalog, TopRepository &repository, QWidget *parent = nullptr);

    /// Recharge la liste des toupies sauvegardées.
    void refresh();

signals:
    void backRequested();
    void topsChanged();

private:
    QComboBox *createPartCombo(int kindIndex);
    Top currentTop() const;
    void updatePreview();
    void loadTop(const Top &top);
    void newTop();
    void saveTop();
    void deleteSelected();

    const PartCatalog &m_catalog;
    TopRepository &m_repository;

    QListWidget *m_list = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_partCombos[3] = {};
    QTableWidget *m_table = nullptr;
    StatsWidget *m_preview = nullptr;
    QLabel *m_status = nullptr;
};

} // namespace toupie
