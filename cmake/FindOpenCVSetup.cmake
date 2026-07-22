# OpenCV integration for Selt_Gui (Qt MinGW 64-bit)
#
# Qt MinGW 不能与 MSVC 版 OpenCV (vc16) 混用。
# 使用 Qt 同款 MinGW 编译并 install 后，设置 OpenCV_DIR，例如：
#   D:/OpenCV/opencv/opencv-mingw-build/install/x64/mingw/lib

option(SELT_ENABLE_VISION "Build vision/OpenCV modules when available" ON)
option(SELT_REQUIRE_OPENCV "Fail configure if OpenCV is not found" OFF)

if(NOT SELT_ENABLE_VISION)
    message(STATUS "SELT_ENABLE_VISION=OFF, skipping OpenCV")
    set(SELT_HAS_OPENCV FALSE)
    return()
endif()

# 候选 MinGW 路径（按优先级；含本机实际 install 布局）
set(_selt_opencv_candidate_dirs
    "D:/OpenCV/opencv/opencv-mingw-build/install/x64/mingw/lib"
    "D:/OpenCV/opencv/opencv-mingw-build/install"
    "D:/OpenCV/opencv/opencv-mingw-build"
    "D:/OpenCV/opencv-mingw-build/install/x64/mingw/lib"
    "D:/OpenCV/opencv-mingw-build/lib/cmake/opencv4"
    "D:/OpenCV/opencv-mingw-build"
    "C:/msys64/mingw64/lib/cmake/opencv4"
    "C:/msys64/ucrt64/lib/cmake/opencv4"
)

# 环境变量 OpenCV_DIR（用户已配置时优先）
if(NOT OpenCV_DIR AND DEFINED ENV{OpenCV_DIR} AND NOT "$ENV{OpenCV_DIR}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{OpenCV_DIR}" _selt_env_opencv_dir)
    set(OpenCV_DIR "${_selt_env_opencv_dir}" CACHE PATH "Path to OpenCVConfig.cmake directory")
    message(STATUS "Using OpenCV_DIR from environment: ${OpenCV_DIR}")
endif()

# 若缓存中的 OpenCV_DIR 指向 MSVC 安装，则清除
if(DEFINED OpenCV_DIR)
    if(OpenCV_DIR MATCHES "vc16" OR OpenCV_DIR MATCHES "[/\\\\]build[/\\\\]x64[/\\\\]vc")
        message(WARNING
            "已忽略不兼容的 OpenCV_DIR: ${OpenCV_DIR}\n"
            "该路径为 MSVC 版本，不能与 Qt MinGW 一起使用。")
        unset(OpenCV_DIR CACHE)
    endif()
endif()

# 未手动指定时，从 MinGW 候选路径中选取
if(NOT OpenCV_DIR)
    foreach(_dir IN LISTS _selt_opencv_candidate_dirs)
        if(EXISTS "${_dir}/OpenCVConfig.cmake")
            set(OpenCV_DIR "${_dir}" CACHE PATH "Path to OpenCVConfig.cmake directory")
            break()
        endif()
    endforeach()
endif()

# 仅在候选路径 / 显式 OpenCV_DIR 中查找，避免误用 MSVC 官方包
set(_selt_opencv_search_paths ${_selt_opencv_candidate_dirs})
if(OpenCV_DIR)
    list(PREPEND _selt_opencv_search_paths "${OpenCV_DIR}")
endif()

find_package(OpenCV QUIET
    COMPONENTS core imgproc imgcodecs
    OPTIONAL_COMPONENTS videoio features2d objdetect
    PATHS ${_selt_opencv_search_paths}
    NO_DEFAULT_PATH
)

if(NOT OpenCV_FOUND)
    set(SELT_HAS_OPENCV FALSE)
    set(SELT_HAS_OPENCV_VIDEOIO FALSE)
    message(WARNING
        "未找到与 Qt MinGW 兼容的 OpenCV，视觉模块将被禁用。\n"
        "当前仍可编译节点画布编辑器。\n"
        "\n"
        "启用视觉功能请:\n"
        "  1. 使用 Qt 同款 MinGW 编译 OpenCV 4.12\n"
        "  2. install 后设置:\n"
        "     OpenCV_DIR=D:/OpenCV/opencv/opencv-mingw-build/install/x64/mingw/lib\n"
        "  3. PATH 加入:\n"
        "     D:/OpenCV/opencv/opencv-mingw-build/install/x64/mingw/bin\n"
        "  4. 重新 Configure\n"
        "\n"
        "不要使用 .../opencv/build/x64/vc16 (MSVC)。")
    if(SELT_REQUIRE_OPENCV)
        message(FATAL_ERROR "SELT_REQUIRE_OPENCV=ON 但未找到 OpenCV，配置中止。")
    endif()
    return()
endif()

set(SELT_HAS_OPENCV TRUE)

# Camera capture needs opencv_videoio. Prefer CMake component; fall back to lib search.
set(SELT_HAS_OPENCV_VIDEOIO FALSE)
if(OpenCV_videoio_FOUND OR TARGET opencv_videoio)
    set(SELT_HAS_OPENCV_VIDEOIO TRUE)
endif()
if(NOT SELT_HAS_OPENCV_VIDEOIO AND DEFINED OpenCV_LIB_COMPONENTS)
    if("videoio" IN_LIST OpenCV_LIB_COMPONENTS)
        set(SELT_HAS_OPENCV_VIDEOIO TRUE)
    endif()
endif()
if(NOT SELT_HAS_OPENCV_VIDEOIO)
    # Some MinGW installs expose the lib without registering the component cleanly.
    foreach(_lib ${OpenCV_LIBS})
        if(_lib MATCHES "opencv_videoio")
            set(SELT_HAS_OPENCV_VIDEOIO TRUE)
            break()
        endif()
    endforeach()
endif()
if(NOT SELT_HAS_OPENCV_VIDEOIO)
    # OpenCVConfig.cmake lives under .../lib; DLLs/libs may sit next to it or one level up.
    set(_selt_videoio_search_paths
        ${OpenCV_LIB_PATH}
        ${OpenCV_LIBRARY_DIRS}
        "${OpenCV_DIR}"
        "${OpenCV_DIR}/.."
        "${OpenCV_DIR}/../lib"
        "${OpenCV_DIR}/../../lib"
        "${OpenCV_DIR}/../x64/mingw/lib"
        "${OpenCV_DIR}/../../x64/mingw/lib"
    )
    find_library(SELT_OPENCV_VIDEOIO_LIB
        NAMES
            opencv_videoio
            opencv_videoio4
            libopencv_videoio
            libopencv_videoio.dll.a
            opencv_videoio4120
            opencv_videoio4110
            opencv_videoio4100
            opencv_videoio490
            opencv_videoio480
        PATHS ${_selt_videoio_search_paths}
        NO_DEFAULT_PATH
    )
    if(NOT SELT_OPENCV_VIDEOIO_LIB)
        find_library(SELT_OPENCV_VIDEOIO_LIB
            NAMES opencv_videoio opencv_videoio4 libopencv_videoio
            PATHS ${_selt_videoio_search_paths}
        )
    endif()
    if(SELT_OPENCV_VIDEOIO_LIB)
        list(APPEND OpenCV_LIBS "${SELT_OPENCV_VIDEOIO_LIB}")
        set(SELT_HAS_OPENCV_VIDEOIO TRUE)
        message(STATUS "Found OpenCV videoio library: ${SELT_OPENCV_VIDEOIO_LIB}")
    endif()
endif()

if(SELT_HAS_OPENCV_VIDEOIO)
    message(STATUS "OpenCV videoio: enabled (camera capture available)")
else()
    message(WARNING
        "OpenCV videoio 未找到：相机抓帧将不可用，目录回放/图片输入仍可用。\n"
        "若需要相机，请用 WITH_FFMPEG 或默认选项重新编译/安装带 videoio 的 OpenCV。")
endif()

# Feature matching (ORB / BFMatcher) needs opencv_features2d.
set(SELT_HAS_OPENCV_FEATURES2D FALSE)
if(OpenCV_features2d_FOUND OR TARGET opencv_features2d)
    set(SELT_HAS_OPENCV_FEATURES2D TRUE)
endif()
if(NOT SELT_HAS_OPENCV_FEATURES2D AND DEFINED OpenCV_LIB_COMPONENTS)
    if("features2d" IN_LIST OpenCV_LIB_COMPONENTS)
        set(SELT_HAS_OPENCV_FEATURES2D TRUE)
    endif()
endif()
if(NOT SELT_HAS_OPENCV_FEATURES2D)
    foreach(_lib ${OpenCV_LIBS})
        if(_lib MATCHES "opencv_features2d")
            set(SELT_HAS_OPENCV_FEATURES2D TRUE)
            break()
        endif()
    endforeach()
endif()
if(NOT SELT_HAS_OPENCV_FEATURES2D)
    set(_selt_features2d_search_paths
        ${OpenCV_LIB_PATH}
        ${OpenCV_LIBRARY_DIRS}
        "${OpenCV_DIR}"
        "${OpenCV_DIR}/.."
        "${OpenCV_DIR}/../lib"
        "${OpenCV_DIR}/../../lib"
        "${OpenCV_DIR}/../x64/mingw/lib"
        "${OpenCV_DIR}/../../x64/mingw/lib"
    )
    find_library(SELT_OPENCV_FEATURES2D_LIB
        NAMES
            opencv_features2d
            opencv_features2d4
            libopencv_features2d
            libopencv_features2d.dll.a
            opencv_features2d4120
            opencv_features2d4110
            opencv_features2d4100
            opencv_features2d490
            opencv_features2d480
        PATHS ${_selt_features2d_search_paths}
        NO_DEFAULT_PATH
    )
    if(NOT SELT_OPENCV_FEATURES2D_LIB)
        find_library(SELT_OPENCV_FEATURES2D_LIB
            NAMES opencv_features2d opencv_features2d4 libopencv_features2d
            PATHS ${_selt_features2d_search_paths}
        )
    endif()
    if(SELT_OPENCV_FEATURES2D_LIB)
        list(APPEND OpenCV_LIBS "${SELT_OPENCV_FEATURES2D_LIB}")
        set(SELT_HAS_OPENCV_FEATURES2D TRUE)
        message(STATUS "Found OpenCV features2d library: ${SELT_OPENCV_FEATURES2D_LIB}")
    endif()
endif()

if(SELT_HAS_OPENCV_FEATURES2D)
    message(STATUS "OpenCV features2d: enabled (ORB feature match available)")
else()
    message(WARNING
        "OpenCV features2d 未找到：特征匹配节点将无法链接。\n"
        "请确认 MinGW OpenCV 安装包含 opencv_features2d，并重新 Configure。")
endif()

# Homography RANSAC (feature match) needs opencv_calib3d.
# Only mark enabled when the library is actually appended to OpenCV_LIBS / linked.
set(SELT_HAS_OPENCV_CALIB3D FALSE)
foreach(_lib ${OpenCV_LIBS})
    if(_lib MATCHES "opencv_calib3d")
        set(SELT_HAS_OPENCV_CALIB3D TRUE)
        break()
    endif()
endforeach()
if(NOT SELT_HAS_OPENCV_CALIB3D AND TARGET opencv_calib3d)
    list(APPEND OpenCV_LIBS opencv_calib3d)
    set(SELT_HAS_OPENCV_CALIB3D TRUE)
    message(STATUS "Linking OpenCV calib3d target: opencv_calib3d")
endif()
if(NOT SELT_HAS_OPENCV_CALIB3D)
    set(_selt_calib3d_search_paths
        ${OpenCV_LIB_PATH}
        ${OpenCV_LIBRARY_DIRS}
        "${OpenCV_DIR}"
        "${OpenCV_DIR}/.."
        "${OpenCV_DIR}/../lib"
        "${OpenCV_DIR}/../../lib"
        "${OpenCV_DIR}/../x64/mingw/lib"
        "${OpenCV_DIR}/../../x64/mingw/lib"
    )
    find_library(SELT_OPENCV_CALIB3D_LIB
        NAMES
            opencv_calib3d
            opencv_calib3d4
            libopencv_calib3d
            libopencv_calib3d.dll.a
            opencv_calib3d4120
            opencv_calib3d4110
            opencv_calib3d4100
            opencv_calib3d490
            opencv_calib3d480
        PATHS ${_selt_calib3d_search_paths}
        NO_DEFAULT_PATH
    )
    if(NOT SELT_OPENCV_CALIB3D_LIB)
        find_library(SELT_OPENCV_CALIB3D_LIB
            NAMES opencv_calib3d opencv_calib3d4 libopencv_calib3d
            PATHS ${_selt_calib3d_search_paths}
        )
    endif()
    if(SELT_OPENCV_CALIB3D_LIB)
        list(APPEND OpenCV_LIBS "${SELT_OPENCV_CALIB3D_LIB}")
        set(SELT_HAS_OPENCV_CALIB3D TRUE)
        message(STATUS "Found OpenCV calib3d library: ${SELT_OPENCV_CALIB3D_LIB}")
    endif()
endif()

if(SELT_HAS_OPENCV_CALIB3D)
    message(STATUS "OpenCV calib3d: enabled (homography RANSAC available)")
else()
    message(WARNING
        "OpenCV calib3d 未找到：特征匹配将回退到无单应性的质心估计。\n"
        "请确认 MinGW OpenCV 安装包含 opencv_calib3d，并重新 Configure。")
endif()

# QR / barcode recognition needs opencv_objdetect (+ optional barcode API).
set(SELT_HAS_OPENCV_OBJDETECT FALSE)
if(OpenCV_objdetect_FOUND OR TARGET opencv_objdetect)
    set(SELT_HAS_OPENCV_OBJDETECT TRUE)
endif()
if(NOT SELT_HAS_OPENCV_OBJDETECT AND DEFINED OpenCV_LIB_COMPONENTS)
    if("objdetect" IN_LIST OpenCV_LIB_COMPONENTS)
        set(SELT_HAS_OPENCV_OBJDETECT TRUE)
    endif()
endif()
if(NOT SELT_HAS_OPENCV_OBJDETECT)
    foreach(_lib ${OpenCV_LIBS})
        if(_lib MATCHES "opencv_objdetect")
            set(SELT_HAS_OPENCV_OBJDETECT TRUE)
            break()
        endif()
    endforeach()
endif()
if(NOT SELT_HAS_OPENCV_OBJDETECT)
    set(_selt_objdetect_search_paths
        ${OpenCV_LIB_PATH}
        ${OpenCV_LIBRARY_DIRS}
        "${OpenCV_DIR}"
        "${OpenCV_DIR}/.."
        "${OpenCV_DIR}/../lib"
        "${OpenCV_DIR}/../../lib"
        "${OpenCV_DIR}/../x64/mingw/lib"
        "${OpenCV_DIR}/../../x64/mingw/lib"
    )
    find_library(SELT_OPENCV_OBJDETECT_LIB
        NAMES
            opencv_objdetect
            opencv_objdetect4
            libopencv_objdetect
            libopencv_objdetect.dll.a
            opencv_objdetect4120
            opencv_objdetect4110
            opencv_objdetect4100
            opencv_objdetect490
            opencv_objdetect480
        PATHS ${_selt_objdetect_search_paths}
        NO_DEFAULT_PATH
    )
    if(NOT SELT_OPENCV_OBJDETECT_LIB)
        find_library(SELT_OPENCV_OBJDETECT_LIB
            NAMES opencv_objdetect opencv_objdetect4 libopencv_objdetect
            PATHS ${_selt_objdetect_search_paths}
        )
    endif()
    if(SELT_OPENCV_OBJDETECT_LIB)
        list(APPEND OpenCV_LIBS "${SELT_OPENCV_OBJDETECT_LIB}")
        set(SELT_HAS_OPENCV_OBJDETECT TRUE)
        message(STATUS "Found OpenCV objdetect library: ${SELT_OPENCV_OBJDETECT_LIB}")
    endif()
endif()

# QRCodeDetector lives in objdetect headers.
set(SELT_HAS_OPENCV_QR FALSE)
if(SELT_HAS_OPENCV_OBJDETECT)
    find_path(SELT_OPENCV_OBJDETECT_HEADER
        NAMES opencv2/objdetect.hpp
        PATHS ${OpenCV_INCLUDE_DIRS}
        NO_DEFAULT_PATH
    )
    if(NOT SELT_OPENCV_OBJDETECT_HEADER)
        find_path(SELT_OPENCV_OBJDETECT_HEADER NAMES opencv2/objdetect.hpp)
    endif()
    if(SELT_OPENCV_OBJDETECT_HEADER)
        set(SELT_HAS_OPENCV_QR TRUE)
    endif()
endif()

# BarcodeDetector is optional (OpenCV contrib / newer objdetect).
set(SELT_HAS_OPENCV_BARCODE FALSE)
if(SELT_HAS_OPENCV_OBJDETECT)
    find_path(SELT_OPENCV_BARCODE_HEADER
        NAMES opencv2/objdetect/barcode.hpp opencv2/barcode.hpp
        PATHS ${OpenCV_INCLUDE_DIRS}
        NO_DEFAULT_PATH
    )
    if(NOT SELT_OPENCV_BARCODE_HEADER)
        find_path(SELT_OPENCV_BARCODE_HEADER NAMES opencv2/objdetect/barcode.hpp opencv2/barcode.hpp)
    endif()
    if(SELT_OPENCV_BARCODE_HEADER)
        set(SELT_HAS_OPENCV_BARCODE TRUE)
    endif()
endif()

if(SELT_HAS_OPENCV_QR)
    message(STATUS "OpenCV QR: enabled (QRCodeDetector available)")
else()
    message(WARNING "OpenCV QR: disabled (缺少 objdetect / QRCodeDetector)")
endif()
if(SELT_HAS_OPENCV_BARCODE)
    message(STATUS "OpenCV barcode: enabled (BarcodeDetector available)")
else()
    message(STATUS "OpenCV barcode: disabled (缺少 barcode 头文件；二维码仍可用)")
endif()

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV_DIR: ${OpenCV_DIR}")
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
