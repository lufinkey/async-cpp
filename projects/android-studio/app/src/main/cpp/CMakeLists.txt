
cmake_minimum_required(VERSION 3.18.1)
project(TestApp)
set( CMAKE_CXX_STANDARD 20 )

set( ASYNCCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." )
set( DATACPP_ROOT "${ASYNCCPP_ROOT}/../data-cpp" )

add_subdirectory( "${DATACPP_ROOT}/projects/android-studio/libdatacpp/src/main/cpp" DataCpp_build )
add_subdirectory( "${ASYNCCPP_ROOT}/projects/android-studio/libasynccpp/src/main/cpp" AsyncCpp_build )

add_library( TestApp SHARED
		native-lib.cpp
		"${ASYNCCPP_ROOT}/src/test/AsyncCppTests.cpp"
		"${DATACPP_ROOT}/src/test/DataCppTests.cpp" )

find_library( log-lib log )

target_link_libraries(
		TestApp

		DataCpp
		AsyncCpp
		${log-lib} )
