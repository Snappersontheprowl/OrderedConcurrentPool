cmake_minimum_required(VERSION 3.20)

project(
  OrderedConcurrentPool
  VERSION 0.1.0
  DESCRIPTION "C++17 ordered concurrent worker pool"
  LANGUAGES CXX
)

include(GNUInstallDirs)

find_package(Threads REQUIRED)

option(ORDERED_CONCURRENT_POOL_BUILD_TESTS "Build OrderedConcurrentPool tests" ON)
option(ORDERED_CONCURRENT_POOL_BUILD_EXAMPLES "Build OrderedConcurrentPool examples" ON)

add_library(ordered_concurrent_pool INTERFACE)
add_library(ocp::ordered_concurrent_pool ALIAS ordered_concurrent_pool)

target_compile_features(ordered_concurrent_pool INTERFACE cxx_std_17)

target_include_directories(ordered_concurrent_pool
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(ordered_concurrent_pool
  INTERFACE
    Threads::Threads
)

if(ORDERED_CONCURRENT_POOL_BUILD_EXAMPLES)
  add_executable(ordered_concurrent_pool_basic_batch
    examples/basic_batch.cpp
  )

  target_link_libraries(ordered_concurrent_pool_basic_batch
    PRIVATE
      ocp::ordered_concurrent_pool
  )
endif()

if(ORDERED_CONCURRENT_POOL_BUILD_TESTS)
  enable_testing()

  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(ordered_concurrent_pool_test
    tests/ordered_concurrent_pool_test.cpp
  )

  target_link_libraries(ordered_concurrent_pool_test
    PRIVATE
      ocp::ordered_concurrent_pool
      GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(ordered_concurrent_pool_test)
endif()

install(
  TARGETS ordered_concurrent_pool
  EXPORT OrderedConcurrentPoolTargets
)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  EXPORT OrderedConcurrentPoolTargets
  NAMESPACE ocp::
  FILE OrderedConcurrentPoolTargets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OrderedConcurrentPool
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
  cmake/OrderedConcurrentPoolConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/OrderedConcurrentPoolConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OrderedConcurrentPool
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/OrderedConcurrentPoolConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/OrderedConcurrentPoolConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/OrderedConcurrentPoolConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OrderedConcurrentPool
)
