#pragma once

#include <QWidget>

class QLabel;

namespace toupie {

/// Aide : récapitulatif des touches et de la souris (d'après les paramètres).
class HelpPage : public QWidget
{
    Q_OBJECT

public:
    explicit HelpPage(QWidget *parent = nullptr);

    /// Régénère le récapitulatif avec les touches actuelles.
    void refresh();

signals:
    void backRequested();
    void settingsRequested();

private:
    QLabel *m_text = nullptr;
};

} // namespace toupie
