# Using an installed GTest package that's not been built with same compiler may result
# in undefined symbols linker errors.
# find_package(GTest CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB Test_SRCS *.cpp)

# Add an executable to the project using specified source files.
# See details at: https://cmake.org/cmake/help/v3.1/command/add_executable.html
add_executable(tests ${Test_SRCS})

# Link a target to given libraries.
# See details at: https://cmake.org/cmake/help/v3.1/command/target_link_libraries.html
#target_link_libraries(tests flow::flow GTest::gtest GTest::gtest_main)
target_link_libraries(tests flow::flow GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)
