#pragma once

#include <QWidget>

#include <array>

class QLabel;
class QPushButton;

namespace toupie {

/// Paramètres : touches du clavier et boutons de la souris associés à
/// chaque action (cliquer sur une case puis appuyer sur la touche voulue).
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    /// Réaffiche les associations actuelles.
    void refresh();

signals:
    void backRequested();

private:
    std::array<std::array<QPushButton *, 2>, 8> m_buttons{};
    QLabel *m_status = nullptr;
};

} // namespace toupie
