cmake_minimum_required(VERSION 3.19)
project(Selt_Gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Test)

qt_standard_project_setup()

set(SELT_CORE_SOURCES
    core/model/nodestyle.cpp
    core/model/portmodel.cpp
    core/model/nodemodel.cpp
    core/model/connectionmodel.cpp
    core/model/documentsettings.cpp
    core/model/document.cpp
    core/model/document.h
    core/serialization/documentserializer.cpp
    core/registry/nodefactory.cpp
    core/command/documentcommands.cpp
)

set(SELT_GRAPHICS_SOURCES
    graphics/items/portgraphicsitem.cpp
    graphics/items/nodegraphicsitem.cpp
    graphics/items/nodegraphicsitem.h
    graphics/items/connectiongraphicsitem.cpp
    graphics/canvas/canvasscene.cpp
    graphics/canvas/canvasscene.h
    graphics/canvas/canvasview.cpp
    graphics/canvas/canvasview.h
)

set(SELT_UI_SOURCES
    ui/nodepalette.cpp
    ui/nodepalette.h
    ui/propertypanel.cpp
    ui/propertypanel.h
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
)

qt_add_executable(Selt_Gui
    WIN32 MACOSX_BUNDLE
    app/main.cpp
    ${SELT_CORE_SOURCES}
    ${SELT_GRAPHICS_SOURCES}
    ${SELT_UI_SOURCES}
)

target_include_directories(Selt_Gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_property(TARGET Selt_Gui PROPERTY AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_link_libraries(Selt_Gui
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

# ---- unit tests (core only) ----
qt_add_executable(Selt_Gui_tests
    tests/test_document.cpp
    ${SELT_CORE_SOURCES}
)

target_include_directories(Selt_Gui_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Selt_Gui_tests
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Test
        Qt::Widgets
)

install(TARGETS Selt_Gui
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Selt_Gui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
