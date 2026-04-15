cmake_minimum_required(VERSION 3.16)
project(pingplusplus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Portable output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Qt 6.5+
set(QT_MIN_VERSION 6.5)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Gui)

# Qt XML for xlsx parsing (QuaZip uses it internally)
find_package(Qt6 QUIET COMPONENTS Xml)

# XlsxWriter
find_package(XlsxWriter QUIET)
if(NOT XlsxWriter_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(XLSXWRITER QUIET libxlsxwriter)
    endif()
    if(NOT XLSXWRITER_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            xlsxwriter
            GIT_REPOSITORY https://github.com/jmcnamara/xlsxwriter.git
            GIT_TAG main
        )
        FetchContent_MakeAvailable(xlsxwriter)
    endif()
endif()

# QuaZip for xlsx reading
find_package(QuaZip QUIET)
if(NOT QuaZip_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(QUAZIP QUIET quazip5-qt6)
    endif()
    if(NOT QUAZIP_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            quazip
            GIT_REPOSITORY https://github.com/stachenov/quazip.git
            GIT_TAG v1.5
        )
        FetchContent_MakeAvailable(quazip)
    endif()
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
elseif(QUAZIP_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QUAZIP_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${QUAZIP_INCLUDE_DIRS})
endif()

# Windows-specific libs (raw ICMP)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 iphlpapi)
endif()

# Install rules
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Tests
enable_testing()
add_subdirectory(tests)
