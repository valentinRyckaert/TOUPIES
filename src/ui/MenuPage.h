#pragma once

#include <QWidget>

namespace toupie {

/// Écran d'accueil.
class MenuPage : public QWidget
{
    Q_OBJECT

public:
    explicit MenuPage(QWidget *parent = nullptr);

signals:
    void playRequested();
    void lanRequested();
    void tournamentRequested();
    void builderRequested();
    void updateRequested();
    void quitRequested();
};

} // namespace toupie
