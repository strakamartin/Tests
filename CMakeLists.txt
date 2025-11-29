cmake_minimum_required(VERSION 3.5)

project(QtTestMaker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets Sql REQUIRED)

add_executable(QtTestMaker
    main.cpp
    models.h
    dbmanager.h
    dbmanager.cpp
    mainwindow.h
    mainwindow.cpp
    testrunner.h
    testrunner.cpp
)

target_link_libraries(QtTestMaker PRIVATE Qt5::Widgets Qt5::Sql)
