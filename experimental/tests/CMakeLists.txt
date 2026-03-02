cmake_minimum_required(VERSION 3.30)

project(ui_tests)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Prevent override of parent compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ui_tests
  ui_tests.cpp

  ../forms.h
  ../forms.cpp
  ../grid.h
  ../grid.cpp
  ../builders.h
  ../builders.cpp
  ../primatives.h
  ../primatives.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:Release>:NDEBUG>
)

target_link_libraries(ui_tests PRIVATE gtest_main SynthEditLib)

include(GoogleTest)
gtest_discover_tests(ui_tests)
