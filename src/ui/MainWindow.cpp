#include "ui/MainWindow.h"

#include "core/PartCatalog.h"
#include "session/LocalSession.h"
#include "ui/BattlePage.h"
#include "ui/BuilderPage.h"
#include "ui/MenuPage.h"
#include "ui/SetupPage.h"

#include <QApplication>
#include <QMessageBox>
#include <QStackedWidget>
#include <QTimer>

namespace toupie {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_repository(PartCatalog::instance())
{
    setWindowTitle(tr("Toupie Arena"));
    resize(900, 860);

    const bool loaded = m_repository.load();

    m_stack = new QStackedWidget(this);
    m_menu = new MenuPage(m_stack);
    m_builder = new BuilderPage(PartCatalog::instance(), m_repository, m_stack);
    m_setup = new SetupPage(m_repository, m_stack);
    m_battle = new BattlePage(m_stack);
    for (QWidget *page : {static_cast<QWidget *>(m_menu), static_cast<QWidget *>(m_builder),
                          static_cast<QWidget *>(m_setup), static_cast<QWidget *>(m_battle)})
        m_stack->addWidget(page);
    setCentralWidget(m_stack);

    connect(m_menu, &MenuPage::playRequested, this, &MainWindow::showSetup);
    connect(m_menu, &MenuPage::builderRequested, this, &MainWindow::showBuilder);
    connect(m_menu, &MenuPage::quitRequested, qApp, &QApplication::quit);
    connect(m_builder, &BuilderPage::backRequested, this, &MainWindow::showMenu);
    connect(m_builder, &BuilderPage::topsChanged, m_setup, &SetupPage::refresh);
    connect(m_setup, &SetupPage::backRequested, this, &MainWindow::showMenu);
    connect(m_setup, &SetupPage::fightRequested, this,
            [this](const Top &player, const Top &opponent, Difficulty difficulty,
                   const QString &stadiumId) {
                const Stadium stadium = Stadium::find(stadiumId).value_or(Stadium::classic());
                auto session = std::make_unique<LocalSession>(
                    QList<Top>{player, opponent},
                    QStringList{tr("Vous"), tr("Bot %1").arg(toString(difficulty))}, stadium,
                    difficulty);
                m_stack->setCurrentWidget(m_battle);
                m_battle->start(std::move(session), BattlePage::Mode::Solo);
            });
    connect(m_battle, &BattlePage::leaveRequested, this, &MainWindow::showMenu);
    connect(m_battle, &BattlePage::setupRequested, this, &MainWindow::showSetup);
    connect(m_battle, &BattlePage::rematchRequested, this, [this] {
        if (auto *local = qobject_cast<LocalSession *>(m_battle->session()))
            local->restart();
    });

    showMenu();

    if (!loaded) {
        QTimer::singleShot(0, this, [this] {
            QMessageBox::warning(this, tr("Toupies sauvegardées"),
                                 tr("Le fichier des toupies est illisible et a été ignoré :\n%1")
                                     .arg(m_repository.filePath()));
        });
    }
}

void MainWindow::showMenu()
{
    m_battle->stop();
    m_stack->setCurrentWidget(m_menu);
}

void MainWindow::showBuilder()
{
    m_battle->stop();
    m_builder->refresh();
    m_stack->setCurrentWidget(m_builder);
}

void MainWindow::showSetup()
{
    m_battle->stop();
    m_setup->refresh();
    m_stack->setCurrentWidget(m_setup);
}

} // namespace toupie
