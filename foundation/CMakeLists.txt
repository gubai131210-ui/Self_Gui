# Selt Foundation — shared utilities (IDs, errors, versioning helpers)
set(SELT_FOUNDATION_SOURCES
    ${CMAKE_SOURCE_DIR}/foundation/seltversion.cpp
    ${CMAKE_SOURCE_DIR}/foundation/selterror.cpp
    ${CMAKE_SOURCE_DIR}/foundation/seltid.cpp
    ${CMAKE_SOURCE_DIR}/foundation/seltlogging.cpp
    ${CMAKE_SOURCE_DIR}/foundation/appsettings.cpp
)

add_library(SeltFoundation STATIC ${SELT_FOUNDATION_SOURCES})
target_include_directories(SeltFoundation PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(SeltFoundation PUBLIC Qt::Core)
set_target_properties(SeltFoundation PROPERTIES AUTOMOC OFF)

target_compile_features(SeltFoundation PUBLIC cxx_std_17)
