cmake_minimum_required (VERSION 2.6) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set_property(GLOBAL PROPERTY CXX_STANDARD 11)
set_property(GLOBAL PROPERTY CXX_STANDARD_REQUIRED true)
 
set (PROJECT final) 

project (${PROJECT} CXX)

#include_directories (../)

if (MSVC)
	add_definitions (/W4)
elseif (CMAKE_COMPILER_IS_GNUCXX)
	add_definitions (-Wall -pedantic -std=c++11)
else ()
	message ("Unknown compiler")
endif ()


set (HEADERS )

set (SOURCES 
	main.cpp)

source_group ("Source Files" FILES ${SOURCES})
 
add_executable (${PROJECT} ${SOURCES} ${HEADERS})
