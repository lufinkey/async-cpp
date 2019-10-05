# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.4.1)
set( CMAKE_CXX_STANDARD 17 )

set( ASYNCCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." )

include_directories(
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${ASYNCCPP_ROOT}/src"
		"${ASYNCCPP_ROOT}/../data-cpp/src")

add_subdirectory(
		"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../data-cpp/projects/android-studio/libdatacpp/src/main/cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/../../../build/libdatacpp")

file(GLOB_RECURSE ASYNCCPP_SOURCES ../../../../../../src/fgl/*.cpp)

add_library(AsyncCpp STATIC
		AsyncCpp_android.cpp
		${ASYNCCPP_SOURCES})

find_library(
		log-lib
		log)

target_link_libraries(
		AsyncCpp

		DataCpp
		${log-lib})
