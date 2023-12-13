cmake_minimum_required(VERSION 3.24)

project(balmbench LANGUAGES C VERSION 1.0.0)

find_package(Python3 3.12 REQUIRED COMPONENTS Development.Module)

Python3_add_library(balmbench MODULE)
target_link_libraries(balmbench PRIVATE Python3::Module)

if(
  (CMAKE_C_COMPILER_ID STREQUAL "Clang") AND
  (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
)
  target_compile_options(balmbench PRIVATE "/clang:-std=gnu2x")
else()
  target_compile_features(balmbench PRIVATE c_std_23)
endif()

if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
  target_compile_options(balmbench PRIVATE "-Wall")
endif()

add_subdirectory(src)

install(
  TARGETS balmbench
  LIBRARY DESTINATION ${PY_BUILD_CMAKE_PACKAGE_NAME}-${PY_BUILD_CMAKE_PACKAGE_VERSION}.data/platlib
)
