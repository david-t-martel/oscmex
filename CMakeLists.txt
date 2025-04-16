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
