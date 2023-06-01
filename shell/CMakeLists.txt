find_package(LibEdit REQUIRED)
find_package(Boost)
if(Boost_FOUND)
	target_include_directories(flow SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
endif()

file(GLOB SHELL_SRCS *.cpp)

# Add an executable to the project using specified source files.
# See details at: https://cmake.org/cmake/help/v3.1/command/add_executable.html
add_executable(shell ${SHELL_SRCS})

message(STATUS "Adding ${libedit_INCLUDE_DIRS}")

target_include_directories(shell PRIVATE ${libedit_INCLUDE_DIRS})

# Link a target to given libraries.
# See details at: https://cmake.org/cmake/help/v3.1/command/target_link_libraries.html
target_link_libraries(shell flow::flow ${libedit_LIBRARIES})

