#pragma once

#include "core/Top.h"

#include <QList>
#include <QWidget>

class QBoxLayout;
class QGroupBox;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace toupie {

class PartCatalog;
class TopRepository;

/// Carte d'un emplacement de l'atelier : dessin de la pièce choisie, nom, stats et
/// flèches ‹ › ; un clic sur la carte ouvre la liste illustrée des pièces.
class PartSlot : public QWidget
{
    Q_OBJECT

public:
    PartSlot(PartKind kind, const QList<Part> &parts, const QColor &color, QWidget *parent = nullptr);

    PartKind kind() const { return m_kind; }
    const Part &part() const { return m_parts.at(m_index); }
    int index() const { return m_index; }
    /// Change de pièce (bornée, en boucle) et émet changed() si elle diffère.
    void setIndex(int index);
    void selectId(const QString &id);

    QSize sizeHint() const override { return {220, 150}; }
    QSize minimumSizeHint() const override { return {180, 150}; }

signals:
    void changed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRectF arrowRect(int direction) const;
    void openList();

    PartKind m_kind;
    QList<Part> m_parts;
    QColor m_color;
    int m_index = 0;
};

/// Atelier d'assemblage : choix des quatre pièces, vue éclatée, aperçu en
/// rotation, statistiques et gestion des toupies sauvegardées.
class BuilderPage : public QWidget
{
    Q_OBJECT

public:
    BuilderPage(const PartCatalog &catalog, TopRepository &repository, QWidget *parent = nullptr);

    /// Recharge la liste des toupies (pré-assemblées puis sauvegardées).
    void refresh();

signals:
    void backRequested();
    void topsChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    Top currentTop() const;
    void updatePreview();
    void loadTop(const Top &top);
    void newTop();
    void saveTop();
    void deleteSelected();

    const PartCatalog &m_catalog;
    TopRepository &m_repository;
    QList<Top> m_tops;    ///< Pré-assemblées puis personnelles (étoilées).
    int m_presetCount = 0;

    QGroupBox *m_listBox = nullptr;
    QGroupBox *m_benchBox = nullptr;
    QListWidget *m_list = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QBoxLayout *m_listButtons = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QList<PartSlot *> m_slots;
    QWidget *m_bench = nullptr;
    QWidget *m_spin = nullptr;
    QWidget *m_stats = nullptr;
};

} // namespace toupie
