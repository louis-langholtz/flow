find_package(Boost)

file(GLOB FLOW_SRCS "source/flow/*.cpp")
file(GLOB FLOW_HDRS "include/flow/*.hpp")
file(GLOB EXT_SRCS "source/ext/*.cpp")
file(GLOB EXT_HDRS "include/ext/*.hpp")

if (${FLOW_ENABLE_COVERAGE} AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	message(STATUS "lib: Adding definitions for coverage analysis.")
	add_definitions(--coverage)
endif()

set(libsrc
	${FLOW_SRCS}
	${FLOW_HDRS}
	${EXT_SRCS}
	${EXT_HDRS}
)

add_library(flow ${libsrc})
add_library(flow::flow ALIAS flow)

if(Boost_FOUND)
	target_include_directories(flow SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
endif()

target_compile_options(flow PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -Werror>
  $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -Werror>
  $<$<CXX_COMPILER_ID:AppleClang>:-Wall -Wextra -Wpedantic -Werror>
)

target_compile_features(flow PUBLIC cxx_std_20)
set_target_properties(flow PROPERTIES
	OUTPUT_NAME "flow"
	VERSION ${FLOW_VERSION}
	SOVERSION ${FLOW_VERSION})
target_include_directories(flow PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/>"
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
