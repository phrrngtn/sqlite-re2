cmake_minimum_required(VERSION 3.0)
project(sqlite-re2)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_path(SQLITE_INCLUDE_DIR sqlite3ext.h)
include_directories(${SQLITE_INCLUDE_DIR})

find_package(nlohmann_json CONFIG REQUIRED)
find_package(re2 CONFIG REQUIRED)

add_executable(re2_test re2_test.cpp)
target_link_libraries(re2_test PRIVATE re2::re2)

add_library(sqlite_re2 SHARED sqlite_re2.cpp)
target_link_libraries(sqlite_re2 PUBLIC re2::re2)