cmake_minimum_required (VERSION 3.0)

#set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(DEBUG_MODE "Enable debug flags and others" OFF)

#Some overdone optimization flags.
#This cause compile fail on some ARM devices. turn this off if this cause your compile to fail
option(NATIVE_COMPILE "Compile based on CPU native instruction support" ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#Some over-engineered compile flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
if(DEBUG_MODE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O -g")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -funroll-loops")

	#GCC only flags, these does not wotk on clang(on Ubuntu), icc isn't tested...
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -pipe")
	endif()
endif()

if(NATIVE_COMPILE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

add_subdirectory(FlatConcert)
