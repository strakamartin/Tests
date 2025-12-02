cmake_minimum_required(VERSION 3.16)

project(QtTestMaker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automoc/autouic/autorcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6
find_package(Qt6 COMPONENTS Widgets Sql REQUIRED)

add_executable(QtTestMaker
    main.cpp
    mainwindow.cpp
    dbmanager.cpp
    testrunner.cpp
    # headers can be listed too (helpful for IDEs), not required for build
    mainwindow.h
    dbmanager.h
    models.h
    testrunner.h
    README.md
)

target_link_libraries(QtTestMaker PRIVATE
    Qt6::Widgets
    Qt6::Sql
)
