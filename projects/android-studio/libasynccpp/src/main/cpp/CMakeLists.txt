
cmake_minimum_required(VERSION 3.18.1)
project(AsyncCpp)
set( CMAKE_CXX_STANDARD 20 )

set( ASYNCCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." )
set( DATACPP_ROOT "${ASYNCCPP_ROOT}/../data-cpp" )
set( DATACPP_LIB_DIR "${DATACPP_ROOT}/projects/android-studio/libdatacpp/build/intermediates/cmake/${CMAKE_BUILD_TYPE}/obj/${ANDROID_ABI}" )

execute_process(COMMAND "${ASYNCCPP_ROOT}/tools/fetch_dependencies.sh")

file(GLOB_RECURSE ASYNCCPP_SOURCES "${ASYNCCPP_ROOT}/src/fgl/*.cpp")

add_library(AsyncCpp SHARED
		_unused.cpp
		${ASYNCCPP_SOURCES})

target_include_directories(
		AsyncCpp
		PUBLIC
		"${ASYNCCPP_ROOT}/src"
		"${ASYNCCPP_ROOT}/../dtl"
		"${ASYNCCPP_ROOT}/external/dtl"
		"${ASYNCCPP_ROOT}/../data-cpp/src")

find_library(log-lib log)

target_link_libraries(
		AsyncCpp

		DataCpp
		${log-lib})
