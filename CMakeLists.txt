# This is a configuration file for CMake.
# Commands herein described at: https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html

# Minimally require a CMake version that can handle what's necessary...
# According to the following link, support for cxx_std_17 first became available in CMake 3.8.
# https://github.com/Kitware/CMake/blob/07cfb18f9d29cfc0588ede928846a03ec5599c48/Help/release/3.8.rst
# Availability of CMAKE_PROJECT_VERSION first appeared in CMake 3.12
cmake_minimum_required(VERSION 3.16.3)

foreach(p
    CMP0048 # OK to clear PROJECT_VERSION on project()
    CMP0054 # CMake 3.1
    CMP0056 # export EXE_LINKER_FLAGS to try_run
    CMP0057 # Support no if() IN_LIST operator
    CMP0063 # Honor visibility properties for all targets
    CMP0077 # Allow option() overrides in importing projects
    )
  if(POLICY ${p})
    cmake_policy(SET ${p} NEW)
  endif()
endforeach()

# Set name for entire project. This establishes the project name in <PROJECT_NAME>_* variables.
# Details at: https://cmake.org/cmake/help/latest/command/project.html#command:project
project(flow
        VERSION 0.0.1
	DESCRIPTION "Dataflow engine and library."
        HOMEPAGE_URL "https://louis-langholtz.github.io/flow/"
)

# Make sure we can import our CMake functions
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Read the git tags to determine the project version
include(GetGitVersion)
get_git_version(GIT_VERSION)

# Tell the user what versions we are using
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" VERSION ${GIT_VERSION})
message(STATUS "Version: ${VERSION}")

# Require C++17...
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Defaults the CMAKE_BUILD_TYPE to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Semicolon separated list of supported configuration types, only
# supports Debug, Release, MinSizeRel, and RelWithDebInfo, anything
# else will be ignored.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;" CACHE STRING "Semicolon separated list of supported configuration types.")

# Tell C++ compiler to optimize release builds for speed.
# In clang++, the optimize for speed flag is '-Ot'. This option isn't supported on g++
# however and it'd be nice to use an option that works for both compilers. So use '-O3'.
# For Visual Studio Compilers, the speed optimization flag is /O2, which is the default setting for release builds
if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
else()
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)

# Provide options that user can optionally select.
# Include the install rules if the user wanted them (included by default when top-level)
# Details at: https://cmake.org/cmake/help/v3.1/command/option.html
option(FLOW_BUILD_SHELL "Build flow shell console application." OFF)
option(FLOW_BUILD_UNITTESTS "Build flow unit tests console application." OFF)
option(FLOW_ENABLE_COVERAGE "Enable code coverage generation." OFF)

set(LIB_INSTALL_DIR lib${LIB_SUFFIX})

# Tell Microsoft Visual C (MSVC) to enable unwind semantics since it doesn't by default.
# For info on MSVC Exception Handling Model see:
#   https://msdn.microsoft.com/en-us/library/1deeycx5.aspx
# Perhaps this should be a build option for all platforms?
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -EHsc")
endif()

# Make sure Microsoft Visual C++ (MSVC) uses the standards-conforming compiler behavior.
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_subdirectory(library)

# Shell console application.
if(FLOW_BUILD_SHELL)
  # Have CMake produce a "shell" make target.
  add_subdirectory(shell)
endif(FLOW_BUILD_SHELL)

# Unit tests console application.
if(FLOW_BUILD_UNITTESTS)
  # Have CMake produce a "test" make target.
  enable_testing()
  add_subdirectory(tests)
endif(FLOW_BUILD_UNITTESTS)

