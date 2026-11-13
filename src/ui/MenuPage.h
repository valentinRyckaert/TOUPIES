#pragma once

#include <QWidget>

namespace toupie {

class TopRepository;

/// Écran d'accueil.
class MenuPage : public QWidget
{
    Q_OBJECT

public:
    /// @param repository  toupies pré-assemblées et perso, tirées au sort pour le combat
    ///                    de bots en fond de menu
    explicit MenuPage(const TopRepository &repository, QWidget *parent = nullptr);

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
