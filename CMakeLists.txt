cmake_minimum_required(VERSION 3.15)
project(AudioEngine VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Intel compiler detection
include(CheckCXXCompiler)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(TestBigEndian)

# Find Intel oneAPI components
find_package(TBB REQUIRED)
find_package(IPP REQUIRED)
find_package(MKL REQUIRED) # Add this line if using MKL

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- liblo Configuration ---
option(LIBLO_ENABLE_THREADS "Build liblo with threading support" ON)

set(LIBLO_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/address.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/blob.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/bundle.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/message.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/method.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/pattern_match.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/send.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/server.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/timetag.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/lo/version.c"
)
set(LIBLO_COMPILE_DEFINITIONS "")
set(LIBLO_LINK_LIBS "")

# Threading
if(LIBLO_ENABLE_THREADS)
	find_package(Threads REQUIRED)
	list(APPEND LIBLO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/lo/server_thread.c")
	list(APPEND LIBLO_LINK_LIBS Threads::Threads)

	if(CMAKE_USE_WIN32_THREADS_INIT)
		list(APPEND LIBLO_COMPILE_DEFINITIONS HAVE_WIN32_THREADS)
	elseif(CMAKE_USE_PTHREADS_INIT)
		list(APPEND LIBLO_COMPILE_DEFINITIONS HAVE_LIBPTHREAD)
	endif()
endif()

# System Checks
check_symbol_exists(poll "poll.h" HAVE_POLL)
check_symbol_exists(select "sys/select.h" HAVE_SELECT)
check_symbol_exists(getifaddrs "sys/types.h;ifaddrs.h" HAVE_GETIFADDRS)
check_symbol_exists(inet_pton "arpa/inet.h" HAVE_INET_PTON)

if(HAVE_POLL)
	list(APPEND LIBLO_COMPILE_DEFINITIONS HAVE_POLL)
endif()

if(HAVE_SELECT)
	list(APPEND LIBLO_COMPILE_DEFINITIONS HAVE_SELECT)
endif()

if(HAVE_GETIFADDRS)
	list(APPEND LIBLO_COMPILE_DEFINITIONS HAVE_GETIFADDRS)
endif()

if(HAVE_INET_PTON)
	list(APPEND LIBLO_COMPILE_DEFINITIONS HAVE_INET_PTON)
endif()

# Endianness
TEST_BIG_ENDIAN(LIBLO_BIGENDIAN_RESULT)

if(LIBLO_BIGENDIAN_RESULT)
	list(APPEND LIBLO_COMPILE_DEFINITIONS LO_BIGENDIAN=1)
else()
	list(APPEND LIBLO_COMPILE_DEFINITIONS LO_BIGENDIAN=0)
endif()

# Platform Libs
if(WIN32)
	list(APPEND LIBLO_LINK_LIBS ws2_32 iphlpapi)
	list(APPEND LIBLO_COMPILE_DEFINITIONS
		_CRT_SECURE_NO_WARNINGS
		_CRT_SECURE_NO_DEPRECATE
		_CRT_NONSTDC_NO_DEPRECATE
		_WINSOCK_DEPRECATED_NO_WARNINGS
		HAVE_WIN32_THREADS
	)
else()
	list(APPEND LIBLO_LINK_LIBS m)
endif()

add_library(liblo STATIC ${LIBLO_SOURCES})
target_include_directories(liblo PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lo>"
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)
target_compile_definitions(liblo PRIVATE ${LIBLO_COMPILE_DEFINITIONS})
target_link_libraries(liblo PRIVATE ${LIBLO_LINK_LIBS})

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
	MKL::mkl_intel_lp64 # Or appropriate MKL interface library
	MKL::mkl_sequential # Or MKL::mkl_tbb_thread
	MKL::mkl_core
)

# Apply optimizations globally if using Intel compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
	add_compile_options(
		-xHost # Optimize for the build machine's architecture
		-O3 # High optimization level

		# -qopenmp      # Enable OpenMP only if you use #pragma omp
		-ipo # Interprocedural optimization (can increase link time)
		-fp-model fast=2 # Faster, potentially less precise FP math
	)

	# For C code as well
	add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wall>) # Example C flag
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options(
		-O3
		-march=native

		# -fopenmp      # Enable OpenMP only if you use #pragma omp
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
