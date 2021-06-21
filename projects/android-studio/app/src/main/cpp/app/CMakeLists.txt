
cmake_minimum_required(VERSION 3.4.1)

set( ASYNCCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." )

add_library( native-lib SHARED native-lib.cpp )

find_library( log-lib log )

target_link_libraries(
		native-lib

		DataCpp
		AsyncCpp
		${log-lib} )
