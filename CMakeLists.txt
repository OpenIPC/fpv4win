cmake_minimum_required(VERSION 3.20)
project(fpv-wfb)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(PkgConfig REQUIRED)

find_package(FFmpeg REQUIRED)
find_package(SDL2 REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
include_directories(${FFMPEG_INCLUDE_DIRS})

find_package(Qt5 COMPONENTS Quick Multimedia  REQUIRED)


file(GLOB_RECURSE SRC_LIST
        3rd/rtl8812au-monitor-pcap/*.c
        3rd/rtl8812au-monitor-pcap/*.cpp
        src/*.cpp
        src/*.c
        src/*.h
)
list(FILTER SRC_LIST EXCLUDE REGEX ".*/3rd.*/src/main.cpp")

include_directories(3rd/rtl8812au-monitor-pcap/src 3rd/rtl8812au-monitor-pcap/hal)
add_executable(${PROJECT_NAME} main.cpp ${SRC_LIST} qml.qrc)

target_link_libraries(${PROJECT_NAME}
        Qt5::Quick ${FFMPEG_LIBRARIES}
        ${SDL2_LIBRARIES}
        PkgConfig::LIBUSB
        unofficial-sodium::sodium)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB Core)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt5${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
