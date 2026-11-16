cmake_minimum_required(VERSION 3.21)
project(Convert VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets)
qt_add_executable(convert WIN32
    svn/main.cpp svn/mainwindow.cpp svn/mainwindow.h
    svn/jobs.cpp svn/jobs.h svn/runner.cpp svn/runner.h
    svn/aboutdialog.cpp svn/aboutdialog.h svn/resources.qrc)
if(WIN32)
    enable_language(RC)
    target_sources(convert PRIVATE svn/convert.rc)
    target_include_directories(convert PRIVATE "${CMAKE_SOURCE_DIR}/svn")
    add_custom_command(TARGET convert POST_BUILD
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/prepare-runtime.ps1"
                -RuntimeDirectory "$<TARGET_FILE_DIR:convert>"
        VERBATIM)
endif()
target_link_libraries(convert PRIVATE Qt6::Widgets)
if(MINGW)
    target_compile_options(convert PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_target_properties(convert PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/bin")
