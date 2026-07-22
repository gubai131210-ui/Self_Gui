# Optional Tesseract OCR for Selt_Gui recognition operators.
# Configure with:
#   Tesseract_ROOT=D:/Tesseract-OCR
#   TESSDATA_PREFIX=D:/Tesseract-OCR/tessdata
# Language packs (*.traineddata) are NOT shipped with the repo.

option(SELT_ENABLE_TESSERACT "Enable Tesseract OCR when available" ON)

set(SELT_HAS_TESSERACT FALSE)
set(SELT_TESSERACT_INCLUDE_DIRS "")
set(SELT_TESSERACT_LIBRARIES "")
set(SELT_TESSERACT_TESSDATA "")

if(NOT SELT_ENABLE_TESSERACT)
    message(STATUS "SELT_ENABLE_TESSERACT=OFF, OCR backend disabled")
    return()
endif()

set(_selt_tesseract_roots
    "$ENV{Tesseract_ROOT}"
    "$ENV{TESSERACT_ROOT}"
    "D:/Tesseract-OCR"
    "C:/Program Files/Tesseract-OCR"
    "C:/msys64/mingw64"
    "C:/msys64/ucrt64"
)
if(DEFINED Tesseract_ROOT AND NOT "${Tesseract_ROOT}" STREQUAL "")
    list(PREPEND _selt_tesseract_roots "${Tesseract_ROOT}")
endif()

find_path(SELT_TESSERACT_INCLUDE_DIR
    NAMES tesseract/baseapi.h
    PATHS ${_selt_tesseract_roots}
    PATH_SUFFIXES include
)
find_library(SELT_TESSERACT_LIBRARY
    NAMES tesseract libtesseract tesseract53 tesseract54 tesseract55
    PATHS ${_selt_tesseract_roots}
    PATH_SUFFIXES lib lib64
)
find_library(SELT_LEPTONICA_LIBRARY
    NAMES lept leptonica liblept libleptonica
    PATHS ${_selt_tesseract_roots}
    PATH_SUFFIXES lib lib64
)

if(DEFINED ENV{TESSDATA_PREFIX} AND NOT "$ENV{TESSDATA_PREFIX}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{TESSDATA_PREFIX}" SELT_TESSERACT_TESSDATA)
endif()
if(SELT_TESSERACT_TESSDATA STREQUAL "" AND SELT_TESSERACT_INCLUDE_DIR)
    get_filename_component(_selt_tess_root "${SELT_TESSERACT_INCLUDE_DIR}" DIRECTORY)
    if(EXISTS "${_selt_tess_root}/tessdata")
        set(SELT_TESSERACT_TESSDATA "${_selt_tess_root}/tessdata")
    elseif(EXISTS "${_selt_tess_root}/share/tessdata")
        set(SELT_TESSERACT_TESSDATA "${_selt_tess_root}/share/tessdata")
    endif()
endif()

if(SELT_TESSERACT_INCLUDE_DIR AND SELT_TESSERACT_LIBRARY)
    set(SELT_HAS_TESSERACT TRUE)
    set(SELT_TESSERACT_INCLUDE_DIRS "${SELT_TESSERACT_INCLUDE_DIR}")
    set(SELT_TESSERACT_LIBRARIES "${SELT_TESSERACT_LIBRARY}")
    if(SELT_LEPTONICA_LIBRARY)
        list(APPEND SELT_TESSERACT_LIBRARIES "${SELT_LEPTONICA_LIBRARY}")
    endif()
    message(STATUS "Tesseract OCR: enabled")
    message(STATUS "  include: ${SELT_TESSERACT_INCLUDE_DIRS}")
    message(STATUS "  libs: ${SELT_TESSERACT_LIBRARIES}")
    if(NOT SELT_TESSERACT_TESSDATA STREQUAL "")
        message(STATUS "  tessdata: ${SELT_TESSERACT_TESSDATA}")
    else()
        message(STATUS "  tessdata: (unset; pass tessdataPath param or TESSDATA_PREFIX)")
    endif()
else()
    message(STATUS "Tesseract OCR: disabled (未找到 tesseract/baseapi.h 或库；OCR 节点将报告能力受限)")
endif()
