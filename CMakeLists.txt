cmake_minimum_required(VERSION 3.19)
project(Selt_Gui VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg Test)
find_package(Qt6 6.5 COMPONENTS Network QUIET)
find_package(Qt6 6.5 COMPONENTS SerialPort QUIET)
find_package(Qt6 6.5 COMPONENTS SerialBus QUIET)

set(SELT_HAS_QT_NETWORK 0)
set(SELT_HAS_QT_SERIALPORT 0)
set(SELT_HAS_QT_SERIALBUS 0)
if(TARGET Qt6::Network)
    set(SELT_HAS_QT_NETWORK 1)
endif()
if(TARGET Qt6::SerialPort)
    set(SELT_HAS_QT_SERIALPORT 1)
endif()
if(TARGET Qt6::SerialBus)
    set(SELT_HAS_QT_SERIALBUS 1)
endif()
message(STATUS "Selt_Gui: Qt Network=${SELT_HAS_QT_NETWORK} SerialPort=${SELT_HAS_QT_SERIALPORT} SerialBus=${SELT_HAS_QT_SERIALBUS}")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindOpenCVSetup.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindTesseractSetup.cmake)

qt_standard_project_setup()
enable_testing()

# Ensure .qrc files are compiled into targets (icons/themes).
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(SELT_HAS_OPENCV)
    get_filename_component(SELT_OPENCV_BIN_DIR "${OpenCV_DIR}/../bin" ABSOLUTE)
endif()

function(selt_configure_test_runtime test_name)
    set(_path_mod "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    if(SELT_HAS_OPENCV AND EXISTS "${SELT_OPENCV_BIN_DIR}")
        set(_path_mod "${_path_mod};PATH=path_list_prepend:${SELT_OPENCV_BIN_DIR}")
    endif()
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT_MODIFICATION "${_path_mod}")
endfunction()

add_subdirectory(foundation)

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

add_library(SeltFlowCore STATIC ${SELT_CORE_SOURCES})
target_include_directories(SeltFlowCore PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(SeltFlowCore PUBLIC Qt::Core Qt::Gui SeltFoundation)
set_target_properties(SeltFlowCore PROPERTIES AUTOMOC ON)

set(SELT_VISION_DATA_SOURCES
    vision/model/visionimage.cpp
    vision/model/measurementresult.cpp
    vision/model/measurementdefinition.cpp
    vision/model/calibrationmodel.cpp
    vision/model/calibrationstore.cpp
    vision/model/visioncontext.cpp
    vision/storage/recipe.cpp
)

set(SELT_VISION_ALGORITHM_SOURCES
    vision/algorithms/imageloader.cpp
    vision/algorithms/grayscalealgorithm.cpp
    vision/algorithms/thresholdalgorithm.cpp
    vision/algorithms/rectanglemeasurementalgorithm.cpp
    vision/algorithms/circlemeasurementalgorithm.cpp
    vision/algorithms/linemeasurementalgorithm.cpp
    vision/algorithms/imagefilteralgorithms.cpp
    vision/algorithms/roiapplier.cpp
    vision/algorithms/preprocessalgorithms.cpp
    vision/algorithms/regionalgorithms.cpp
    vision/algorithms/locatealgorithms.cpp
    vision/algorithms/caliperalgorithm.cpp
    vision/algorithms/subpixelalgorithms.cpp
    vision/algorithms/recognitionalgorithms.cpp
)

set(SELT_VISION_INTEGRATION_SOURCES
    vision/data/datatype.cpp
    vision/model/portexposure.cpp
    vision/registry/visionnoderegistry.cpp
    vision/execution/visionpipeline.cpp
    vision/domain/visionflow.cpp
    vision/domain/visionflow.h
    vision/domain/visionproject.cpp
    vision/domain/visionproject.h
    vision/domain/projectvariables.cpp
    vision/domain/variablescope.cpp
    vision/domain/variablescope.h
    vision/project/projectservice.cpp
    vision/project/projectservice.h
    vision/validation/graphvalidator.cpp
    vision/runtime/portvaluestore.cpp
    vision/runtime/bindingresolver.cpp
    vision/runtime/nodeexecutorregistry.cpp
    vision/runtime/flowruntime.cpp
    vision/runtime/executionscope.cpp
    vision/runtime/flowexecutionsnapshot.cpp
    vision/runtime/flowscheduler.cpp
    vision/runtime/inputsourceservice.cpp
    vision/runtime/samplevalidation.cpp
    vision/runtime/resultsink.cpp
    vision/runtime/runanalysis.cpp
    vision/runtime/runrecord.cpp
    vision/runtime/runcontroller.cpp
    vision/runtime/runcontroller.h
    vision/nodes/builtinexecutors.cpp
    vision/nodes/executors/preprocessexecutors.cpp
    vision/nodes/executors/regionexecutors.cpp
    vision/nodes/executors/locateexecutors.cpp
    vision/nodes/executors/measureexecutors.cpp
    vision/nodes/executors/logicoutputexecutors.cpp
    vision/nodes/executors/recognitionexecutors.cpp
    vision/storage/projectstorage.cpp
    vision/storage/projectcontainer.cpp
    vision/storage/projectresourcestore.cpp
    vision/storage/inspectionhistory.cpp
    vision/services/templateteachservice.cpp
    vision/workflow/workflowtemplates.cpp
    plugin_host/pluginhost.cpp
    devices/camera/icameradevice.cpp
    communication/codec.cpp
    communication/types.cpp
    communication/crc.cpp
    communication/mocktransport.cpp
    communication/clients.cpp
    communication/communicationmanager.cpp
    vision/nodes/executors/communicationexecutors.cpp
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
    ui/nodecatalogmodel.cpp
    ui/nodecatalogmodel.h
    ui/nodeinspectordialog.cpp
    ui/nodeinspectordialog.h
    ui/propertypanel.cpp
    ui/propertypanel.h
    ui/flowbarwidget.cpp
    ui/flowbarwidget.h
    ui/theme/iconprovider.cpp
    ui/theme/iconprovider.h
    ui/theme/themecontroller.cpp
    ui/theme/themecontroller.h
    ui/theme/uistyle.h
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
)

if(SELT_HAS_OPENCV)
    list(APPEND SELT_UI_SOURCES
        ui/bindingeditor.cpp
        ui/bindingeditor.h
        ui/imagepreviewwidget.cpp
        ui/imagepreviewwidget.h
        ui/measurementpanel.cpp
        ui/measurementpanel.h
        ui/calibrationdialog.cpp
        ui/calibrationdialog.h
        ui/runtimemonitorwidget.cpp
        ui/runtimemonitorwidget.h
        ui/inputsourcedock.cpp
        ui/inputsourcedock.h
        ui/resourcebrowserdock.cpp
        ui/resourcebrowserdock.h
        ui/communicationdock.cpp
        ui/communicationdock.h
        ui/variabledock.cpp
        ui/variabledock.h
    )
endif()

set(SELT_APP_SOURCES
    app/main.cpp
    app/nodebuilder.cpp
)

if(SELT_HAS_OPENCV)
    add_library(SeltVisionData STATIC ${SELT_VISION_DATA_SOURCES})
    target_include_directories(SeltVisionData PUBLIC ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV=1)
    if(SELT_HAS_OPENCV_VIDEOIO)
        target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV_VIDEOIO=1)
    else()
        target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV_VIDEOIO=0)
    endif()
    if(SELT_HAS_OPENCV_CALIB3D)
        target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV_CALIB3D=1)
    else()
        target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV_CALIB3D=0)
    endif()
    if(SELT_HAS_OPENCV_QR)
        target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV_QR=1)
    else()
        target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV_QR=0)
    endif()
    if(SELT_HAS_OPENCV_BARCODE)
        target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV_BARCODE=1)
    else()
        target_compile_definitions(SeltVisionData PUBLIC SELT_HAS_OPENCV_BARCODE=0)
    endif()
    target_link_libraries(SeltVisionData PUBLIC Qt::Core Qt::Gui SeltFoundation ${OpenCV_LIBS})

    add_library(SeltVisionRuntime STATIC
        ${SELT_VISION_INTEGRATION_SOURCES}
        ${SELT_VISION_ALGORITHM_SOURCES}
    )
    target_include_directories(SeltVisionRuntime PUBLIC ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV=1)
    if(SELT_HAS_OPENCV_VIDEOIO)
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV_VIDEOIO=1)
    else()
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV_VIDEOIO=0)
    endif()
    if(SELT_HAS_OPENCV_CALIB3D)
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV_CALIB3D=1)
    else()
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV_CALIB3D=0)
    endif()
    if(SELT_HAS_OPENCV_QR)
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV_QR=1)
    else()
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV_QR=0)
    endif()
    if(SELT_HAS_OPENCV_BARCODE)
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV_BARCODE=1)
    else()
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_OPENCV_BARCODE=0)
    endif()
    if(SELT_HAS_TESSERACT)
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_TESSERACT=1)
        if(NOT SELT_TESSERACT_TESSDATA STREQUAL "")
            target_compile_definitions(SeltVisionRuntime PUBLIC
                SELT_DEFAULT_TESSDATA_PATH="${SELT_TESSERACT_TESSDATA}")
        endif()
        target_include_directories(SeltVisionRuntime PUBLIC ${SELT_TESSERACT_INCLUDE_DIRS})
        target_link_libraries(SeltVisionRuntime PUBLIC ${SELT_TESSERACT_LIBRARIES})
    else()
        target_compile_definitions(SeltVisionRuntime PUBLIC SELT_HAS_TESSERACT=0)
    endif()
    target_link_libraries(SeltVisionRuntime PUBLIC SeltVisionData SeltFlowCore Qt::Widgets ${OpenCV_LIBS})
    target_compile_definitions(SeltVisionRuntime PUBLIC
        SELT_HAS_QT_NETWORK=${SELT_HAS_QT_NETWORK}
        SELT_HAS_QT_SERIALPORT=${SELT_HAS_QT_SERIALPORT}
        SELT_HAS_QT_SERIALBUS=${SELT_HAS_QT_SERIALBUS}
    )
    if(SELT_HAS_QT_NETWORK)
        target_link_libraries(SeltVisionRuntime PUBLIC Qt6::Network)
    endif()
    if(SELT_HAS_QT_SERIALPORT)
        target_link_libraries(SeltVisionRuntime PUBLIC Qt6::SerialPort)
    endif()
    if(SELT_HAS_QT_SERIALBUS)
        target_link_libraries(SeltVisionRuntime PUBLIC Qt6::SerialBus)
    endif()
    set_target_properties(SeltVisionRuntime PROPERTIES AUTOMOC ON)
endif()

qt_add_executable(Selt_Gui
    WIN32 MACOSX_BUNDLE
    ${SELT_APP_SOURCES}
    ${SELT_GRAPHICS_SOURCES}
    ${SELT_UI_SOURCES}
)

# Compile resources/selt.qrc → :/selt/icons/... (paths are relative to the .qrc file)
set(SELT_QRC "${CMAKE_CURRENT_SOURCE_DIR}/resources/selt.qrc")
target_sources(Selt_Gui PRIVATE ${SELT_QRC})
set_source_files_properties(${SELT_QRC} PROPERTIES SKIP_AUTORCC OFF)
set_target_properties(Selt_Gui PROPERTIES
    AUTORCC ON
    AUTOMOC ON
    AUTOUIC ON
)

if(SELT_HAS_OPENCV)
    target_compile_definitions(Selt_Gui PRIVATE SELT_HAS_OPENCV=1)
    target_link_libraries(Selt_Gui PRIVATE SeltVisionRuntime)
else()
    target_compile_definitions(Selt_Gui PRIVATE SELT_HAS_OPENCV=0)
    target_link_libraries(Selt_Gui PRIVATE SeltFlowCore)
endif()

target_include_directories(Selt_Gui PRIVATE ${CMAKE_SOURCE_DIR})
set_property(TARGET Selt_Gui PROPERTY AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/ui)
target_link_libraries(Selt_Gui PRIVATE Qt::Core Qt::Gui Qt::Widgets Qt::Svg SeltFoundation)

qt_add_executable(Selt_Gui_tests
    tests/test_document.cpp
)
target_include_directories(Selt_Gui_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(Selt_Gui_tests PRIVATE SeltFlowCore Qt::Test Qt::Widgets)
add_test(NAME Selt_Gui_tests COMMAND Selt_Gui_tests)
selt_configure_test_runtime(Selt_Gui_tests)

qt_add_executable(Selt_Gui_ui_tests
    tests/test_ui_foundation.cpp
    ui/theme/iconprovider.cpp
    ui/theme/themecontroller.cpp
    ui/nodecatalogmodel.cpp
    ui/flowbarwidget.cpp
)
target_sources(Selt_Gui_ui_tests PRIVATE ${SELT_QRC})
target_include_directories(Selt_Gui_ui_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(Selt_Gui_ui_tests PRIVATE SELT_HAS_OPENCV=0)
set_target_properties(Selt_Gui_ui_tests PROPERTIES AUTOMOC ON AUTORCC ON)
target_link_libraries(Selt_Gui_ui_tests PRIVATE Qt::Core Qt::Gui Qt::Widgets Qt::Svg Qt::Test SeltFoundation)
add_test(NAME Selt_Gui_ui_tests COMMAND Selt_Gui_ui_tests)
selt_configure_test_runtime(Selt_Gui_ui_tests)

if(SELT_HAS_OPENCV)
    qt_add_executable(Selt_Gui_vision_tests
        tests/test_vision.cpp
        ${SELT_VISION_DATA_SOURCES}
        ${SELT_VISION_ALGORITHM_SOURCES}
    )
    target_compile_definitions(Selt_Gui_vision_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_vision_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_vision_tests PRIVATE Qt::Core Qt::Gui Qt::Test SeltFoundation ${OpenCV_LIBS})
    add_test(NAME Selt_Gui_vision_tests COMMAND Selt_Gui_vision_tests)
    selt_configure_test_runtime(Selt_Gui_vision_tests)

    qt_add_executable(Selt_Gui_vision_integration_tests
        tests/test_vision_integration.cpp
    )
    target_compile_definitions(Selt_Gui_vision_integration_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_vision_integration_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_vision_integration_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_vision_integration_tests COMMAND Selt_Gui_vision_integration_tests)
    selt_configure_test_runtime(Selt_Gui_vision_integration_tests)

    qt_add_executable(Selt_Gui_roi_applier_tests
        tests/test_roi_applier.cpp
        ${SELT_VISION_DATA_SOURCES}
        ${SELT_VISION_ALGORITHM_SOURCES}
    )
    target_compile_definitions(Selt_Gui_roi_applier_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_roi_applier_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_roi_applier_tests PRIVATE Qt::Core Qt::Gui Qt::Test SeltFoundation ${OpenCV_LIBS})
    add_test(NAME Selt_Gui_roi_applier_tests COMMAND Selt_Gui_roi_applier_tests)
    selt_configure_test_runtime(Selt_Gui_roi_applier_tests)

    qt_add_executable(Selt_Gui_builtin_executor_tests
        tests/test_builtin_executors.cpp
    )
    target_compile_definitions(Selt_Gui_builtin_executor_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_builtin_executor_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_builtin_executor_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_builtin_executor_tests COMMAND Selt_Gui_builtin_executor_tests)
    selt_configure_test_runtime(Selt_Gui_builtin_executor_tests)

    qt_add_executable(Selt_Gui_domain_tests
        tests/test_domain_runtime.cpp
    )
    target_compile_definitions(Selt_Gui_domain_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_domain_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_domain_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_domain_tests COMMAND Selt_Gui_domain_tests)
    selt_configure_test_runtime(Selt_Gui_domain_tests)

    qt_add_executable(Selt_Gui_project_service_tests
        tests/test_project_service.cpp
    )
    target_compile_definitions(Selt_Gui_project_service_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_project_service_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_project_service_tests PRIVATE SeltVisionRuntime Qt::Test Qt::Widgets)
    add_test(NAME Selt_Gui_project_service_tests COMMAND Selt_Gui_project_service_tests)
    selt_configure_test_runtime(Selt_Gui_project_service_tests)

    qt_add_executable(Selt_Gui_data_binding_tests
        tests/test_data_binding.cpp
    )
    target_compile_definitions(Selt_Gui_data_binding_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_data_binding_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_data_binding_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_data_binding_tests COMMAND Selt_Gui_data_binding_tests)
    selt_configure_test_runtime(Selt_Gui_data_binding_tests)

    qt_add_executable(Selt_Gui_graph_validation_tests
        tests/test_graph_validation.cpp
    )
    target_compile_definitions(Selt_Gui_graph_validation_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_graph_validation_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_graph_validation_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_graph_validation_tests COMMAND Selt_Gui_graph_validation_tests)
    selt_configure_test_runtime(Selt_Gui_graph_validation_tests)

    qt_add_executable(Selt_Gui_editor_variable_ux_tests
        tests/test_editor_variable_ux.cpp
        ui/nodepalette.cpp
        ui/nodecatalogmodel.cpp
        ui/theme/iconprovider.cpp
        ui/theme/operatorpreferences.h
    )
    target_compile_definitions(Selt_Gui_editor_variable_ux_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_editor_variable_ux_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    set_target_properties(Selt_Gui_editor_variable_ux_tests PROPERTIES AUTOMOC ON AUTORCC ON)
    target_sources(Selt_Gui_editor_variable_ux_tests PRIVATE ${SELT_QRC})
    target_link_libraries(Selt_Gui_editor_variable_ux_tests PRIVATE SeltVisionRuntime Qt::Test Qt::Widgets Qt::Svg)
    add_test(NAME Selt_Gui_editor_variable_ux_tests COMMAND Selt_Gui_editor_variable_ux_tests)
    selt_configure_test_runtime(Selt_Gui_editor_variable_ux_tests)

    qt_add_executable(Selt_Gui_editor_roadmap_tests
        tests/test_editor_roadmap.cpp
    )
    target_compile_definitions(Selt_Gui_editor_roadmap_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_editor_roadmap_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_editor_roadmap_tests PRIVATE SeltVisionRuntime Qt::Test Qt::Widgets)
    add_test(NAME Selt_Gui_editor_roadmap_tests COMMAND Selt_Gui_editor_roadmap_tests)
    selt_configure_test_runtime(Selt_Gui_editor_roadmap_tests)

    qt_add_executable(Selt_Gui_visionmaster_experience_tests
        tests/test_visionmaster_experience.cpp
    )
    target_compile_definitions(Selt_Gui_visionmaster_experience_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_visionmaster_experience_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_visionmaster_experience_tests PRIVATE SeltVisionRuntime Qt::Test Qt::Widgets)
    add_test(NAME Selt_Gui_visionmaster_experience_tests COMMAND Selt_Gui_visionmaster_experience_tests)
    selt_configure_test_runtime(Selt_Gui_visionmaster_experience_tests)

    qt_add_executable(Selt_Gui_production_hardening_tests
        tests/test_production_hardening.cpp
    )
    target_compile_definitions(Selt_Gui_production_hardening_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_production_hardening_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_production_hardening_tests PRIVATE SeltVisionRuntime Qt::Test Qt::Widgets)
    add_test(NAME Selt_Gui_production_hardening_tests COMMAND Selt_Gui_production_hardening_tests)
    selt_configure_test_runtime(Selt_Gui_production_hardening_tests)

    qt_add_executable(Selt_Gui_communication_tests
        tests/test_communication.cpp
    )
    target_compile_definitions(Selt_Gui_communication_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_communication_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_communication_tests PRIVATE SeltVisionRuntime Qt::Test)
    if(SELT_HAS_QT_NETWORK)
        target_link_libraries(Selt_Gui_communication_tests PRIVATE Qt6::Network)
    endif()
    add_test(NAME Selt_Gui_communication_tests COMMAND Selt_Gui_communication_tests)
    selt_configure_test_runtime(Selt_Gui_communication_tests)

    qt_add_executable(Selt_Gui_runtime_binding_tests
        tests/test_runtime_bindings.cpp
    )
    target_compile_definitions(Selt_Gui_runtime_binding_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_runtime_binding_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_runtime_binding_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_runtime_binding_tests COMMAND Selt_Gui_runtime_binding_tests)
    selt_configure_test_runtime(Selt_Gui_runtime_binding_tests)

    qt_add_executable(Selt_Gui_sample_validation_tests
        tests/test_sample_validation.cpp
    )
    target_compile_definitions(Selt_Gui_sample_validation_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_sample_validation_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_sample_validation_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_sample_validation_tests COMMAND Selt_Gui_sample_validation_tests)
    selt_configure_test_runtime(Selt_Gui_sample_validation_tests)

    qt_add_executable(Selt_Gui_recognition_tests
        tests/test_recognition_executors.cpp
    )
    target_compile_definitions(Selt_Gui_recognition_tests PRIVATE SELT_HAS_OPENCV=1)
    if(SELT_HAS_OPENCV_QR)
        target_compile_definitions(Selt_Gui_recognition_tests PRIVATE SELT_HAS_OPENCV_QR=1)
    else()
        target_compile_definitions(Selt_Gui_recognition_tests PRIVATE SELT_HAS_OPENCV_QR=0)
    endif()
    if(SELT_HAS_OPENCV_BARCODE)
        target_compile_definitions(Selt_Gui_recognition_tests PRIVATE SELT_HAS_OPENCV_BARCODE=1)
    else()
        target_compile_definitions(Selt_Gui_recognition_tests PRIVATE SELT_HAS_OPENCV_BARCODE=0)
    endif()
    if(SELT_HAS_TESSERACT)
        target_compile_definitions(Selt_Gui_recognition_tests PRIVATE SELT_HAS_TESSERACT=1)
    else()
        target_compile_definitions(Selt_Gui_recognition_tests PRIVATE SELT_HAS_TESSERACT=0)
    endif()
    target_include_directories(Selt_Gui_recognition_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_recognition_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_recognition_tests COMMAND Selt_Gui_recognition_tests)
    selt_configure_test_runtime(Selt_Gui_recognition_tests)

    qt_add_executable(Selt_Gui_port_exposure_tests
        tests/test_port_exposure.cpp
    )
    target_compile_definitions(Selt_Gui_port_exposure_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_port_exposure_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_port_exposure_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_port_exposure_tests COMMAND Selt_Gui_port_exposure_tests)
    selt_configure_test_runtime(Selt_Gui_port_exposure_tests)

    qt_add_executable(Selt_Gui_run_record_tests
        tests/test_run_record.cpp
    )
    target_compile_definitions(Selt_Gui_run_record_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_run_record_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_run_record_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_run_record_tests COMMAND Selt_Gui_run_record_tests)
    selt_configure_test_runtime(Selt_Gui_run_record_tests)

    qt_add_executable(Selt_Gui_binding_editor_tests
        tests/test_binding_editor.cpp
        ui/bindingeditor.cpp
        ui/bindingeditor.h
    )
    target_compile_definitions(Selt_Gui_binding_editor_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_binding_editor_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_binding_editor_tests PRIVATE SeltVisionRuntime Qt::Test Qt::Widgets)
    add_test(NAME Selt_Gui_binding_editor_tests COMMAND Selt_Gui_binding_editor_tests)
    selt_configure_test_runtime(Selt_Gui_binding_editor_tests)

    qt_add_executable(Selt_Gui_phase3_tests
        tests/test_phase3_production.cpp
    )
    target_compile_definitions(Selt_Gui_phase3_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_phase3_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_phase3_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_phase3_tests COMMAND Selt_Gui_phase3_tests)
    selt_configure_test_runtime(Selt_Gui_phase3_tests)

    qt_add_executable(Selt_Gui_run_controller_tests
        tests/test_run_controller.cpp
    )
    target_compile_definitions(Selt_Gui_run_controller_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_run_controller_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_run_controller_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_run_controller_tests COMMAND Selt_Gui_run_controller_tests)
    selt_configure_test_runtime(Selt_Gui_run_controller_tests)

    qt_add_executable(Selt_Gui_run_analysis_tests
        tests/test_run_analysis.cpp
    )
    target_compile_definitions(Selt_Gui_run_analysis_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_run_analysis_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_run_analysis_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_run_analysis_tests COMMAND Selt_Gui_run_analysis_tests)
    selt_configure_test_runtime(Selt_Gui_run_analysis_tests)

    qt_add_executable(Selt_Gui_plugin_host_tests
        tests/test_plugin_host.cpp
    )
    target_compile_definitions(Selt_Gui_plugin_host_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_plugin_host_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_plugin_host_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_plugin_host_tests COMMAND Selt_Gui_plugin_host_tests)
    selt_configure_test_runtime(Selt_Gui_plugin_host_tests)

    qt_add_executable(Selt_Gui_input_source_tests
        tests/test_input_source_runtime.cpp
    )
    target_compile_definitions(Selt_Gui_input_source_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_input_source_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_input_source_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_input_source_tests COMMAND Selt_Gui_input_source_tests)
    selt_configure_test_runtime(Selt_Gui_input_source_tests)

    qt_add_executable(Selt_Gui_extended_roi_tests
        tests/test_extended_roi.cpp
    )
    target_compile_definitions(Selt_Gui_extended_roi_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_extended_roi_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_extended_roi_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_extended_roi_tests COMMAND Selt_Gui_extended_roi_tests)
    selt_configure_test_runtime(Selt_Gui_extended_roi_tests)

    qt_add_executable(Selt_Gui_template_teach_tests
        tests/test_template_teach.cpp
    )
    target_compile_definitions(Selt_Gui_template_teach_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_template_teach_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_template_teach_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_template_teach_tests COMMAND Selt_Gui_template_teach_tests)
    selt_configure_test_runtime(Selt_Gui_template_teach_tests)

    qt_add_executable(Selt_Gui_project_resource_tests
        tests/test_project_resource_store.cpp
    )
    target_compile_definitions(Selt_Gui_project_resource_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_project_resource_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_project_resource_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_project_resource_tests COMMAND Selt_Gui_project_resource_tests)
    selt_configure_test_runtime(Selt_Gui_project_resource_tests)

    qt_add_executable(Selt_Gui_project_container_tests
        tests/test_project_container.cpp
    )
    target_compile_definitions(Selt_Gui_project_container_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_project_container_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_project_container_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_project_container_tests COMMAND Selt_Gui_project_container_tests)
    selt_configure_test_runtime(Selt_Gui_project_container_tests)

    qt_add_executable(Selt_Gui_execution_scope_tests
        tests/test_execution_scope.cpp
    )
    target_compile_definitions(Selt_Gui_execution_scope_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_execution_scope_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_execution_scope_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_execution_scope_tests COMMAND Selt_Gui_execution_scope_tests)
    selt_configure_test_runtime(Selt_Gui_execution_scope_tests)

    qt_add_executable(Selt_Gui_resource_migration_tests
        tests/test_resource_migration.cpp
    )
    target_compile_definitions(Selt_Gui_resource_migration_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_resource_migration_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_resource_migration_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_resource_migration_tests COMMAND Selt_Gui_resource_migration_tests)
    selt_configure_test_runtime(Selt_Gui_resource_migration_tests)

    qt_add_executable(Selt_Gui_runtime_guard_tests
        tests/test_runtime_guards.cpp
    )
    target_compile_definitions(Selt_Gui_runtime_guard_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_runtime_guard_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_runtime_guard_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_runtime_guard_tests COMMAND Selt_Gui_runtime_guard_tests)
    selt_configure_test_runtime(Selt_Gui_runtime_guard_tests)

    qt_add_executable(Selt_Gui_plugin_diagnostic_tests
        tests/test_plugin_diagnostics.cpp
    )
    target_compile_definitions(Selt_Gui_plugin_diagnostic_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_plugin_diagnostic_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_plugin_diagnostic_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_plugin_diagnostic_tests COMMAND Selt_Gui_plugin_diagnostic_tests)
    selt_configure_test_runtime(Selt_Gui_plugin_diagnostic_tests)

    qt_add_executable(Selt_Gui_flow_snapshot_tests
        tests/test_flow_snapshot.cpp
    )
    target_compile_definitions(Selt_Gui_flow_snapshot_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_flow_snapshot_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_flow_snapshot_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_flow_snapshot_tests COMMAND Selt_Gui_flow_snapshot_tests)
    selt_configure_test_runtime(Selt_Gui_flow_snapshot_tests)

    qt_add_executable(Selt_Gui_partial_run_controller_tests
        tests/test_partial_run_controller.cpp
    )
    target_compile_definitions(Selt_Gui_partial_run_controller_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_partial_run_controller_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_partial_run_controller_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_partial_run_controller_tests COMMAND Selt_Gui_partial_run_controller_tests)
    selt_configure_test_runtime(Selt_Gui_partial_run_controller_tests)

    qt_add_executable(Selt_Gui_measurement_domain_tests
        tests/test_measurement_domain.cpp
        ${SELT_VISION_DATA_SOURCES}
    )
    target_compile_definitions(Selt_Gui_measurement_domain_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_measurement_domain_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_measurement_domain_tests PRIVATE Qt::Core Qt::Gui Qt::Test SeltFoundation ${OpenCV_LIBS})
    add_test(NAME Selt_Gui_measurement_domain_tests COMMAND Selt_Gui_measurement_domain_tests)
    selt_configure_test_runtime(Selt_Gui_measurement_domain_tests)

    qt_add_executable(Selt_Gui_industrial_measurement_tests
        tests/test_industrial_measurement.cpp
        ${SELT_VISION_DATA_SOURCES}
        ${SELT_VISION_ALGORITHM_SOURCES}
    )
    target_compile_definitions(Selt_Gui_industrial_measurement_tests PRIVATE SELT_HAS_OPENCV=1)
    if(SELT_HAS_OPENCV_CALIB3D)
        target_compile_definitions(Selt_Gui_industrial_measurement_tests PRIVATE SELT_HAS_OPENCV_CALIB3D=1)
    else()
        target_compile_definitions(Selt_Gui_industrial_measurement_tests PRIVATE SELT_HAS_OPENCV_CALIB3D=0)
    endif()
    target_include_directories(Selt_Gui_industrial_measurement_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_industrial_measurement_tests PRIVATE Qt::Core Qt::Gui Qt::Test SeltFoundation ${OpenCV_LIBS})
    add_test(NAME Selt_Gui_industrial_measurement_tests COMMAND Selt_Gui_industrial_measurement_tests)
    selt_configure_test_runtime(Selt_Gui_industrial_measurement_tests)

    qt_add_executable(Selt_Gui_algorithm_factor_tests
        tests/test_algorithm_factors.cpp
    )
    target_compile_definitions(Selt_Gui_algorithm_factor_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_algorithm_factor_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_algorithm_factor_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_algorithm_factor_tests COMMAND Selt_Gui_algorithm_factor_tests)
    selt_configure_test_runtime(Selt_Gui_algorithm_factor_tests)

    qt_add_executable(Selt_Gui_measurement_closed_loop_tests
        tests/test_measurement_closed_loop.cpp
    )
    target_compile_definitions(Selt_Gui_measurement_closed_loop_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_measurement_closed_loop_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_measurement_closed_loop_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_measurement_closed_loop_tests COMMAND Selt_Gui_measurement_closed_loop_tests)
    selt_configure_test_runtime(Selt_Gui_measurement_closed_loop_tests)

    qt_add_executable(Selt_Gui_algorithm_benchmark_tests
        tests/test_algorithm_benchmark.cpp
    )
    target_compile_definitions(Selt_Gui_algorithm_benchmark_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_algorithm_benchmark_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_algorithm_benchmark_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_algorithm_benchmark_tests COMMAND Selt_Gui_algorithm_benchmark_tests)
    selt_configure_test_runtime(Selt_Gui_algorithm_benchmark_tests)

    qt_add_executable(Selt_Gui_result_sink_tests
        tests/test_result_sink.cpp
    )
    target_compile_definitions(Selt_Gui_result_sink_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_result_sink_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_result_sink_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_result_sink_tests COMMAND Selt_Gui_result_sink_tests)
    selt_configure_test_runtime(Selt_Gui_result_sink_tests)

    qt_add_executable(Selt_Gui_recipe_history_tests
        tests/test_recipe_history.cpp
    )
    target_compile_definitions(Selt_Gui_recipe_history_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_recipe_history_tests PRIVATE ${CMAKE_SOURCE_DIR}
                                                                        ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_recipe_history_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_recipe_history_tests COMMAND Selt_Gui_recipe_history_tests)
    selt_configure_test_runtime(Selt_Gui_recipe_history_tests)

    qt_add_executable(Selt_Gui_camera_lifecycle_tests
        tests/test_camera_lifecycle.cpp
    )
    target_compile_definitions(Selt_Gui_camera_lifecycle_tests PRIVATE SELT_HAS_OPENCV=1)
    target_include_directories(Selt_Gui_camera_lifecycle_tests PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Selt_Gui_camera_lifecycle_tests PRIVATE SeltVisionRuntime Qt::Test)
    add_test(NAME Selt_Gui_camera_lifecycle_tests COMMAND Selt_Gui_camera_lifecycle_tests)
    selt_configure_test_runtime(Selt_Gui_camera_lifecycle_tests)

    # Optional example plugin (same ABI as host). Output goes to bin/plugins/nodes/.
    add_library(selt_invert_node MODULE
        plugins/examples/invert_node/invertnodeplugin.cpp
        plugins/examples/invert_node/invertnodeplugin.h
    )
    target_include_directories(selt_invert_node PRIVATE ${CMAKE_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(selt_invert_node PRIVATE SELT_HAS_OPENCV=1)
    target_link_libraries(selt_invert_node PRIVATE SeltVisionRuntime Qt::Core Qt::Gui)
    set_target_properties(selt_invert_node PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/nodes"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/nodes"
        PREFIX ""
    )

    # Ship plugin folder alongside the app when installing (OpenCV DLLs remain manual / PATH).
    install(DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            OPTIONAL)
endif()

install(TARGETS Selt_Gui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Selt_Gui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# Document OpenCV status at configure time for production packaging.
if(SELT_HAS_OPENCV)
    message(STATUS "Selt_Gui: OpenCV vision enabled (SELT_HAS_OPENCV=1)")
    if(SELT_HAS_OPENCV_VIDEOIO)
        message(STATUS "Selt_Gui: OpenCV videoio enabled (camera capture available)")
    else()
        message(STATUS "Selt_Gui: OpenCV videoio disabled (camera capture unavailable; file/replay still work)")
    endif()
    if(SELT_HAS_OPENCV_QR)
        message(STATUS "Selt_Gui: QR recognition enabled")
    else()
        message(STATUS "Selt_Gui: QR recognition disabled")
    endif()
    if(SELT_HAS_OPENCV_BARCODE)
        message(STATUS "Selt_Gui: barcode recognition enabled")
    else()
        message(STATUS "Selt_Gui: barcode recognition disabled (capability_limited when selected)")
    endif()
else()
    message(STATUS "Selt_Gui: OpenCV vision disabled (editor-only shell)")
endif()
if(SELT_HAS_TESSERACT)
    message(STATUS "Selt_Gui: Tesseract OCR enabled")
else()
    message(STATUS "Selt_Gui: Tesseract OCR disabled (OCR node reports capability_limited)")
endif()
