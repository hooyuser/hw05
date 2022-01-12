cmake_minimum_required(VERSION 3.12)
project(hellocmake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB SRC
    "*.cpp"
)

file(GLOB HEADER
    "*.h"
    "*.hpp"
)

add_executable(main ${SRC} ${HEADER})

source_group("Source Files"            FILES ${SRC})
source_group("Header Files"            FILES ${HEADER})

find_package(Threads REQUIRED)
target_link_libraries(main PUBLIC Threads::Threads)
