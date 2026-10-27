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


#set(CMAKE_CXX_FLAGS "-O3 -Wall -Werror=return-type")

message("Building ${CMAKE_BUILD_TYPE} version")

#include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)


set(SOURCES src/drivers/Generic.cpp
            src/drivers/FakeSDL.cpp
            src/TouchDriver.cpp
            src/GestureRecognizer.cpp
            src/OneFingerFSM.cpp
            src/TwoFingerFSM.cpp
            src/Feeder.cpp)

add_library(GestLib STATIC ${SOURCES})
target_include_directories(GestLib PUBLIC ${PROJECT_SOURCE_DIR}/include ${LIBEVDEV_INCLUDE_DIRS})
target_link_libraries(GestLib ${LIBEVDEV_LIBRARIES})

add_executable(examplesimple examples/exampleSimple.cpp)
target_include_directories(examplesimple PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(examplesimple GestLib)

add_executable(examplesdl examples/exampleSDL.cpp)
target_include_directories(examplesdl PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(examplesdl GestLib SDL2::SDL2 SDL2_ttf::SDL2_ttf)

add_executable(examplegeneric examples/exampleGenericDriver.cpp)
target_include_directories(examplegeneric PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(examplegeneric GestLib SDL2::SDL2 SDL2_ttf::SDL2_ttf)
