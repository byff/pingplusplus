cmake_minimum_required(VERSION 3.16)
project(pingplusplus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Portable output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Qt 6.5+
set(QT_MIN_VERSION 6.5)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Gui)
find_package(Qt6 QUIET COMPONENTS Xml)
find_package(Qt6 QUIET COMPONENTS Core5Compat)

# XlsxWriter - use system package
set(XLSXWRITER_FOUND_SYSTEM FALSE)
find_package(XlsxWriter QUIET)
if(NOT XlsxWriter_FOUND)
    find_path(XLSXWRITER_INCLUDE_DIRS NAMES xlsxwriter.h PATHS /usr/include /usr/local/include)
    find_library(XLSXWRITER_LIBRARIES NAMES xlsxwriter PATHS /usr/lib /usr/lib64 /usr/local/lib)
    if(XLSXWRITER_INCLUDE_DIRS AND XLSXWRITER_LIBRARIES)
        set(XLSXWRITER_FOUND TRUE)
        set(XLSXWRITER_FOUND_SYSTEM TRUE)
    endif()
endif()
if(NOT XLSXWRITER_FOUND)
    message(FATAL_ERROR "XlsxWriter not found. Install libxlsxwriter-dev or xlsxwriter via FetchContent.")
endif()

# QuaZip - use system Qt6 package
set(QUAZIP_FOUND_SYSTEM FALSE)
# Qt6 quazip provides QuaZip-Qt6Config.cmake, not QuaZipConfig.cmake
find_package(QuaZip-Qt6 QUIET)
if(NOT QuaZip-Qt6_FOUND)
    # Fallback to standard name
    find_package(QuaZip QUIET)
endif()
if(QuaZip-Qt6_FOUND)
    message(STATUS "Found system QuaZip Qt6")
    set(QuaZip_FOUND TRUE)
    set(QUAZIP_FOUND_SYSTEM TRUE)
elseif(QuaZip_FOUND)
    message(STATUS "Found system QuaZip (Qt5 version)")
endif()

if(NOT QuaZip_FOUND)
    message(FATAL_ERROR "QuaZip not found. Install libquazip1-qt6-dev.")
endif()

# Sources
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/ping/pingengine.cpp
    src/ping/pingworker.cpp
    src/ping/pingresult.cpp
    src/ping/dnscache.cpp
    src/model/resulttablemodel.cpp
    src/import/excelimporter.cpp
    src/export/excelexporter.cpp
    src/config/configmanager.cpp
)

set(HEADERS
    src/mainwindow.h
    src/ping/pingengine.h
    src/ping/pingworker.h
    src/ping/pingresult.h
    src/ping/dnscache.h
    src/model/resulttablemodel.h
    src/import/excelimporter.h
    src/export/excelexporter.h
    src/config/configmanager.h
)

# Windows: GUI subsystem (no console)
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
endif()

# Link Qt
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Gui
)

# Add source directory for headers
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Link XlsxWriter
if(TARGET xlsxwriter)
    target_link_libraries(${PROJECT_NAME} PRIVATE xlsxwriter)
elseif(XLSXWRITER_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${XLSXWRITER_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${XLSXWRITER_INCLUDE_DIRS})
endif()

# Link QuaZip
if(TARGET QuaZip::QuaZip)
    target_link_libraries(${PROJECT_NAME} PRIVATE QuaZip::QuaZip)
else()
    message(FATAL_ERROR "QuaZip::QuaZip target not found")
endif()

# Windows-specific libs (raw ICMP)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 iphlpapi)
endif()

# Install rules
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
