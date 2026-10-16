
cmake_minimum_required(VERSION 3.24.0)

set (This TimeParser)

project(${This} CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

enable_testing()

add_subdirectory(googletest)

set(Headers
	TimeParser.h
)
set(Sources
	TimeParser.cpp
)

add_library(${This} STATIC ${Sources} ${Headers})
target_include_directories(${This} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(test_cases)


