file(GLOB FLOW_SRCS "source/flow/*.cpp")
file(GLOB FLOW_HDRS "include/flow/*.hpp")

if (${FLOW_ENABLE_COVERAGE} AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	message(STATUS "lib: Adding definitions for coverage analysis.")
	add_definitions(--coverage)
endif()

set(libsrc
	${FLOW_SRCS}
	${FLOW_HDRS}
)

add_library(flow ${libsrc})
add_library(flow::flow ALIAS flow)

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
