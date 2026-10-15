cmake_minimum_required(VERSION 3.21)
project(dukkan VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)

qt_add_executable(dukkan
    src/main.cpp
    src/gamecatalog.cpp
    src/gamecatalog.h
    src/storebackend.cpp
    src/storebackend.h
    src/gameartwork.cpp
    src/gameartwork.h
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
        qml/GameIdentity.qml
        qml/ScreenshotView.qml
        qml/GalleryButton.qml
        qml/ProgressTrack.qml
        qml/TransactionProgress.qml
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

include(CTest)
if(BUILD_TESTING)
    add_executable(catalog-backend-test tests/catalog-backend-test.cpp src/storebackend.cpp src/gamecatalog.cpp src/gameartwork.cpp)
    target_include_directories(catalog-backend-test PRIVATE src)
    target_link_libraries(catalog-backend-test PRIVATE Qt6::Core Qt6::Gui Qt6::Network)
    add_test(NAME catalog-backend COMMAND catalog-backend-test)
    set_tests_properties(catalog-backend PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(gamecatalog-test tests/gamecatalog-test.cpp src/gamecatalog.cpp)
    target_include_directories(gamecatalog-test PRIVATE src)
    target_link_libraries(gamecatalog-test PRIVATE Qt6::Core)
    add_test(NAME shared-catalog COMMAND gamecatalog-test)
    add_executable(gameartwork-test tests/gameartwork-test.cpp src/gameartwork.cpp)
    target_include_directories(gameartwork-test PRIVATE src)
    target_link_libraries(gameartwork-test PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME artwork-metadata COMMAND gameartwork-test)

    add_executable(gallery-ui-test tests/gallery-ui-test.cpp)
    target_link_libraries(gallery-ui-test PRIVATE Qt6::Quick Qt6::QuickControls2)
    target_compile_definitions(gallery-ui-test PRIVATE SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME gallery-ui COMMAND gallery-ui-test)
    set_tests_properties(gallery-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
