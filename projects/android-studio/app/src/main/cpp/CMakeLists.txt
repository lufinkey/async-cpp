
project(AsyncCppTestApp)
cmake_minimum_required(VERSION 3.4.1)

set( ASYNCCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." )
set( DATACPP_ROOT "${ASYNCCPP_ROOT}/../data-cpp" )

add_subdirectory( "${DATACPP_ROOT}/projects/android-studio/libdatacpp/src/main/cpp" DataCpp )
add_subdirectory( "${ASYNCCPP_ROOT}/projects/android-studio/libasynccpp/src/main/cpp" AsyncCpp )

add_library( native-lib SHARED native-lib.cpp "${ASYNCCPP_ROOT}/src/test/AsyncCppTests.cpp" )

find_library( log-lib log )

target_link_libraries(
		native-lib

		DataCpp
		AsyncCpp
		${log-lib} )
