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
    void tutorialRequested();
    void builderRequested();
    void updateRequested();
    void settingsRequested();
    void helpRequested();
    void quitRequested();
};

} // namespace toupie
