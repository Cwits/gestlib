cmake_minimum_required(VERSION 3.10)
project(GestLib)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  message("To build release run next command:")
  message("cmake -DCMAKE_BUILD_TYPE=Release .")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

message("Building ${CMAKE_BUILD_TYPE} version")

include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)


set(SOURCES src/TouchDriver.cpp
            src/GestureRecognizer.cpp
            src/OneFingerFSM.cpp)

add_library(GestLib STATIC ${SOURCES})
target_include_directories(GestLib PUBLIC ${LIBEVDEV_INCLUDE_DIRS})
target_link_libraries(GestLib ${LIBEVDEV_LIBRARIES})

add_executable(example examples/main.cpp)
target_include_directories(example PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(example GestLib)
