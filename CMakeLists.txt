cmake_minimum_required(VERSION 3.21)

project(TopArena
    VERSION 1.2.1
    DESCRIPTION "Top Arena : jeu de combat de toupies en 2D (C++/Qt)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Widgets Multimedia)
qt_standard_project_setup()

# shortcut: Qt 6.8.3 lie encore le framework AGL, absent des SDK macOS récents
# (« framework 'AGL' not found ») ; à retirer quand la CI passera à Qt >= 6.9.2.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    get_target_property(_toupie_gl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(_toupie_gl_libs)
        list(FILTER _toupie_gl_libs EXCLUDE REGEX "AGL")
        set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES
            INTERFACE_LINK_LIBRARIES "${_toupie_gl_libs}")
    endif()
endif()

# Version affichée et comparée aux releases (la CI de release la tire du tag).
set(TOUPIE_VERSION "${PROJECT_VERSION}" CACHE STRING "Version de l'application")

option(TOUPIE_WERROR "Traiter les avertissements comme des erreurs" OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8)
    if(TOUPIE_WERROR)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(TOUPIE_WERROR)
        add_compile_options(-Werror)
    endif()
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
