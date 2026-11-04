# Bibliothèque "cœur" : modèle de données (pièces, toupies) sans dépendance graphique
add_library(toupie_core STATIC
    core/Stats.h
    core/Stats.cpp
    core/Part.h
    core/Top.h
    core/Top.cpp
    core/PartCatalog.h
    core/PartCatalog.cpp
    core/TopRepository.h
    core/TopRepository.cpp
    game/Vec2.h
    game/BattleConfig.h
    game/Stadium.h
    game/Stadium.cpp
    game/SpecialMove.h
    game/SpecialMove.cpp
    game/MatchRules.h
    game/MatchRules.cpp
    game/Battle.h
    game/Battle.cpp
    game/BotController.h
    game/BotController.cpp
    game/Tournament.h
    game/Tournament.cpp
    session/MatchSession.h
    session/MatchSession.cpp
    session/LocalSession.h
    session/LocalSession.cpp
)
target_include_directories(toupie_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(toupie_core PUBLIC Qt6::Core)

# Multijoueur en réseau local : lobby, découverte et sessions réseau
add_library(toupie_net STATIC
    net/Protocol.h
    net/Protocol.cpp
    net/LobbyServer.h
    net/LobbyServer.cpp
    net/LobbyClient.h
    net/LobbyClient.cpp
    net/LobbyBrowser.h
    net/LobbyBrowser.cpp
    net/LobbyScanner.h
    net/LobbyScanner.cpp
    net/MatchCodec.h
    net/MatchCodec.cpp
    net/HostSession.h
    net/HostSession.cpp
    net/ClientSession.h
    net/ClientSession.cpp
    net/TournamentSession.h
    net/TournamentSession.cpp
)
target_link_libraries(toupie_net PUBLIC toupie_core Qt6::Network)

# Interface graphique (bibliothèque, pour pouvoir la tester)
add_library(toupie_ui STATIC
    ui/Theme.h
    ui/Theme.cpp
    ui/Widgets.h
    ui/Widgets.cpp
    ui/TopPainter.h
    ui/TopPainter.cpp
    ui/RulesWidget.h
    ui/RulesWidget.cpp
    ui/StadiumPainter.h
    ui/StadiumPainter.cpp
    ui/StatsWidget.h
    ui/StatsWidget.cpp
    ui/ArenaWidget.h
    ui/ArenaWidget.cpp
    ui/MenuPage.h
    ui/MenuPage.cpp
    ui/BuilderPage.h
    ui/BuilderPage.cpp
    ui/SetupPage.h
    ui/SetupPage.cpp
    ui/BattlePage.h
    ui/BattlePage.cpp
    ui/LanMenuPage.h
    ui/LanMenuPage.cpp
    ui/LobbyPage.h
    ui/LobbyPage.cpp
    ui/TournamentSettingsWidget.h
    ui/TournamentSettingsWidget.cpp
    ui/TournamentSetupPage.h
    ui/TournamentSetupPage.cpp
    ui/TournamentPage.h
    ui/TournamentPage.cpp
    ui/MainWindow.h
    ui/MainWindow.cpp
)
target_link_libraries(toupie_ui PUBLIC toupie_net Qt6::Widgets)

# Application
qt_add_executable(toupie_arena main.cpp)
target_link_libraries(toupie_arena PRIVATE toupie_ui)
set_target_properties(toupie_arena PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME "Toupie Arena"
    MACOSX_BUNDLE_GUI_IDENTIFIER "fr.toupiearena.game"
)

include(GNUInstallDirs)
install(TARGETS toupie_arena
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)
