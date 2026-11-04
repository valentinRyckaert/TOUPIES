#pragma once

#include "core/Top.h"

#include <QList>
#include <QWidget>

#include <optional>

class QDialog;
class QPushButton;
class QToolButton;

namespace toupie {

/// Panneau latéral rétractable : un bouton titre replie ou déplie son contenu.
class SidePanel : public QWidget
{
    Q_OBJECT

public:
    SidePanel(const QString &title, QWidget *content, QWidget *parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const;

private:
    QString m_title;
    QToolButton *m_toggle = nullptr;
    QWidget *m_content = nullptr;
};

/// Fenêtre surgissante (non bloquante) qui accueille `content` ; elle reste
/// enfant de `parent`, on l'affiche avec open().
QDialog *createPopup(QWidget *parent, const QString &title, QWidget *content);

/// Grand bouton de choix (titre + description) pour les écrans d'étapes.
QPushButton *createChoiceCard(const QString &title, const QString &description,
                              QWidget *parent);

/// Choix de l'ordre de passage des toupies (indices) ; nullopt si annulé.
std::optional<QList<int>> chooseTopOrder(QWidget *parent, const QList<Top> &tops);

} // namespace toupie
