#pragma once

#include "core/TopRepository.h"

#include <QMainWindow>

class QStackedWidget;

namespace toupie {

class BattlePage;
class BuilderPage;
class MenuPage;
class SetupPage;

/// Fenêtre principale : navigation entre les différents écrans.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void showMenu();
    void showBuilder();
    void showSetup();

    TopRepository m_repository;
    QStackedWidget *m_stack = nullptr;
    MenuPage *m_menu = nullptr;
    BuilderPage *m_builder = nullptr;
    SetupPage *m_setup = nullptr;
    BattlePage *m_battle = nullptr;
};

} // namespace toupie
