cmake_minimum_required(VERSION 3.14)
project(motionsick_logger)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors
)

add_executable(motionsick_logger
    main.cpp
    sensors/socket_receiver.cpp
    sensors/imu_thread.cpp 
    ui/toggle_window.cpp
    logger/database_logger.cpp
    logger/csv_logger.cpp
)

target_link_libraries(
    motionsick_logger 
    PRIVATE
        Qt5::Widgets
        nlohmann_json::nlohmann_json
        SQLite::SQLite3
)
