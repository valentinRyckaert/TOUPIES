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
    game/Battle.h
    game/Battle.cpp
    game/BotController.h
    game/BotController.cpp
    session/MatchSession.h
    session/MatchSession.cpp
    session/LocalSession.h
    session/LocalSession.cpp
)
target_include_directories(toupie_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(toupie_core PUBLIC Qt6::Core)

# Application graphique
qt_add_executable(toupie_arena
    main.cpp
    ui/TopPainter.h
    ui/TopPainter.cpp
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
    ui/MainWindow.h
    ui/MainWindow.cpp
)
target_link_libraries(toupie_arena PRIVATE toupie_core Qt6::Widgets)
set_target_properties(toupie_arena PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
