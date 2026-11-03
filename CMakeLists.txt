cmake_minimum_required(VERSION 3.21)

project(ToupieArena
    VERSION 1.0.0
    DESCRIPTION "Jeu de combat de toupies en 2D (C++/Qt)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Widgets)
qt_standard_project_setup()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

option(TOUPIE_BUILD_TOOLS "Construire les outils (simulateur d'équilibrage)" ON)
if(TOUPIE_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
