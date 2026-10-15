cmake_minimum_required(VERSION 3.21)
project(dukkan VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)

qt_add_executable(dukkan
    src/main.cpp
    src/storebackend.cpp
    src/storebackend.h
    src/gamepadbridge.cpp
    src/gamepadbridge.h
    src/appearanceprovider.cpp
    src/appearanceprovider.h
)

qt_add_resources(dukkan "dukkan_assets"
    PREFIX "/"
    FILES
        qml/Main.qml
        qml/ButtonDiamond.qml
        qml/GameArt.qml
        qml/GameTile.qml
        qml/ControlHint.qml
        data/games.json
        tools/flatpak-transaction.py
)

target_link_libraries(dukkan PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Network
)

option(DUKKAN_USE_SDL2 "Enable native gamepad input through SDL2 when available" ON)
if(DUKKAN_USE_SDL2)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    endif()
    if(SDL2_FOUND)
        target_link_libraries(dukkan PRIVATE PkgConfig::SDL2)
        target_compile_definitions(dukkan PRIVATE DUKKAN_HAS_SDL2=1)
        message(STATUS "Dukkan: SDL2 gamepad input enabled")
    else()
        message(STATUS "Dukkan: SDL2 dev files not found; keyboard navigation enabled, gamepad disabled")
    endif()
endif()

install(TARGETS dukkan RUNTIME DESTINATION bin)
