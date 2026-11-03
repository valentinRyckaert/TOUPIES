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
    game/Battle.h
    game/Battle.cpp
)
target_include_directories(toupie_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(toupie_core PUBLIC Qt6::Core)
