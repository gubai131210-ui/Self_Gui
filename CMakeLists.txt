cmake_minimum_required(VERSION 3.19)
project(Selt_Gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Test)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindOpenCVSetup.cmake)

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

# 算法/数据层：可独立单测，不依赖 Document
set(SELT_VISION_ALGORITHM_SOURCES
    vision/model/visionimage.cpp
    vision/model/measurementresult.cpp
    vision/model/visioncontext.cpp
    vision/algorithms/imageloader.cpp
    vision/algorithms/grayscalealgorithm.cpp
    vision/algorithms/thresholdalgorithm.cpp
    vision/algorithms/rectanglemeasurementalgorithm.cpp
)

# 节点/流水线层：依赖 core 的 Document / NodeModel
set(SELT_VISION_INTEGRATION_SOURCES
    vision/registry/visionnoderegistry.cpp
    vision/execution/visionpipeline.cpp
)

set(SELT_VISION_SOURCES
    ${SELT_VISION_ALGORITHM_SOURCES}
    ${SELT_VISION_INTEGRATION_SOURCES}
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

if(SELT_HAS_OPENCV)
    list(APPEND SELT_UI_SOURCES
        ui/imagepreviewwidget.cpp
        ui/imagepreviewwidget.h
        ui/measurementpanel.cpp
        ui/measurementpanel.h
    )
endif()

set(SELT_APP_SOURCES
    app/main.cpp
    app/nodebuilder.cpp
)

set(SELT_GUI_SOURCES
    ${SELT_APP_SOURCES}
    ${SELT_CORE_SOURCES}
    ${SELT_GRAPHICS_SOURCES}
    ${SELT_UI_SOURCES}
)

if(SELT_HAS_OPENCV)
    list(APPEND SELT_GUI_SOURCES ${SELT_VISION_SOURCES})
endif()

qt_add_executable(Selt_Gui
    WIN32 MACOSX_BUNDLE
    ${SELT_GUI_SOURCES}
)

if(SELT_HAS_OPENCV)
    target_compile_definitions(Selt_Gui PRIVATE SELT_HAS_OPENCV=1)
else()
    target_compile_definitions(Selt_Gui PRIVATE SELT_HAS_OPENCV=0)
endif()

target_include_directories(Selt_Gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(SELT_HAS_OPENCV)
    target_include_directories(Selt_Gui PRIVATE ${OpenCV_INCLUDE_DIRS})
endif()

set_property(TARGET Selt_Gui PROPERTY AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_link_libraries(Selt_Gui
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

if(SELT_HAS_OPENCV)
    target_link_libraries(Selt_Gui PRIVATE ${OpenCV_LIBS})
endif()

# ---- unit tests ----
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

if(SELT_HAS_OPENCV)
    # 算法层单测（不依赖 Document）
    qt_add_executable(Selt_Gui_vision_tests
        tests/test_vision.cpp
        ${SELT_VISION_ALGORITHM_SOURCES}
    )

    target_compile_definitions(Selt_Gui_vision_tests PRIVATE SELT_HAS_OPENCV=1)

    target_include_directories(Selt_Gui_vision_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(Selt_Gui_vision_tests
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Test
            ${OpenCV_LIBS}
    )

    # 集成层单测：注册表 / 参数校验 / 拓扑 / 单次执行 / ROI / Overlay
    qt_add_executable(Selt_Gui_vision_integration_tests
        tests/test_vision_integration.cpp
        ${SELT_CORE_SOURCES}
        ${SELT_VISION_SOURCES}
    )

    target_compile_definitions(Selt_Gui_vision_integration_tests PRIVATE SELT_HAS_OPENCV=1)

    target_include_directories(Selt_Gui_vision_integration_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(Selt_Gui_vision_integration_tests
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Test
            ${OpenCV_LIBS}
    )
endif()

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
