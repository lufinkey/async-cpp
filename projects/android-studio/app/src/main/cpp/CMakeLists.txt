
cmake_minimum_required(VERSION 3.4.1)

project(AsyncCppTestApp)

set( ASYNCCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." )
set( DATACPP_ROOT "${ASYNCCPP_ROOT}/../data-cpp" )

add_subdirectory("${DATACPP_ROOT}/projects/android-studio/libdatacpp/src/main/cpp" "${CMAKE_BINARY_DIR}/data-cpp")
add_subdirectory("${ASYNCCPP_ROOT}/projects/android-studio/libasynccpp/src/main/cpp" "${CMAKE_BINARY_DIR}/async-cpp")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/app" "${CMAKE_BINARY_DIR}/app")
