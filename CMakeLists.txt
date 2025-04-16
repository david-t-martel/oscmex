cmake_minimum_required(VERSION 3.15)
project(AudioEngine VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Intel compiler detection
include(CheckCXXCompiler)

# Find Intel oneAPI components
find_package(TBB REQUIRED)
find_package(IPP REQUIRED)

# For MKL later when needed
# find_package(MKL REQUIRED)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include external dependencies
add_subdirectory(src/cmake) # Include liblo build

# Build liblo library from src/lo
file(GLOB LIBLO_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/*.c"
)
add_library(liblo STATIC ${LIBLO_SOURCES})
target_include_directories(liblo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/lo)

# Include directories
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/src/AudioEngine
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Source files for AudioEngine
file(GLOB AUDIO_ENGINE_SOURCES
	"src/AudioEngine/*.cpp"
	"src/AudioEngine/*.h"
)

# Define AudioEngine library
add_library(AudioEngine SHARED ${AUDIO_ENGINE_SOURCES})

# Link with oneAPI components and liblo
target_link_libraries(AudioEngine PRIVATE
	TBB::tbb
	IPP::ipp
	liblo

	# MKL::mkl_core MKL::mkl_intel_thread MKL::mkl_intel_ilp64  # When needed
)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
	target_compile_options(AudioEngine PRIVATE
		-xHost # Optimize for current architecture
		-O3 # High optimization
		-qopenmp # Enable OpenMP
		-ipo # Interprocedural optimization
		-fp-model fast=2 # Fast floating-point math
	)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(AudioEngine PRIVATE
		-O3
		-march=native
		-fopenmp
	)
endif()

# Define main executable
add_executable(AudioEngineApp src/main.cpp)
target_link_libraries(AudioEngineApp PRIVATE AudioEngine)

# --- Optional: Build Tools ---
option(BUILD_LO_TOOLS "Build liblo command-line tools (oscsend, oscdump)" OFF)

if(BUILD_LO_TOOLS)
	add_executable(oscsend src/tools/oscsend.c)
	target_link_libraries(oscsend PRIVATE liblo)
	target_include_directories(oscsend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lo) # Ensure tools can find lo.h

	add_executable(oscdump src/tools/oscdump.c)
	target_link_libraries(oscdump PRIVATE liblo)
	target_include_directories(oscdump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lo)

	add_executable(oscsendfile src/tools/oscsendfile.c)
	target_link_libraries(oscsendfile PRIVATE liblo)
	target_include_directories(oscsendfile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lo)

	install(TARGETS oscsend oscdump oscsendfile RUNTIME DESTINATION bin)
endif()

# --- Optional: Build Examples ---
option(BUILD_LO_EXAMPLES "Build liblo examples" OFF)

if(BUILD_LO_EXAMPLES)
	# Add example targets similarly to tools if desired
	# Example:
	# add_executable(example_client src/examples/example_client.c)
	# target_link_libraries(example_client PRIVATE liblo)
	# target_include_directories(example_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lo)
	# ... add other examples ...
endif()

# --- Optional: Doxygen Documentation ---
find_package(Doxygen)

if(DOXYGEN_FOUND)
	option(BUILD_DOCUMENTATION "Build Doxygen documentation" OFF)

	if(BUILD_DOCUMENTATION)
		set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/reference.doxygen)
		set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/doc/html)

		add_custom_target(doc ALL
			COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			COMMENT "Generating API documentation with Doxygen"
			VERBATIM)

		# Note: This is a basic integration. More robust Doxygen setup might be needed.
	endif()
endif()

# Install targets
install(TARGETS AudioEngine AudioEngineApp
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

# Install headers
install(DIRECTORY src/AudioEngine/
	DESTINATION include/AudioEngine
	FILES_MATCHING PATTERN "*.h"
)
