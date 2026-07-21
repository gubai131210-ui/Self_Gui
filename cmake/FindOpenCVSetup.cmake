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

find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs
    PATHS ${_selt_opencv_search_paths}
    NO_DEFAULT_PATH
)

if(NOT OpenCV_FOUND)
    set(SELT_HAS_OPENCV FALSE)
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
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV_DIR: ${OpenCV_DIR}")
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
