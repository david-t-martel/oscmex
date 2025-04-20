cmake_minimum_required(VERSION 3.15)
project(AudioEngine VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories - moved out of root directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable Intel compiler detection - improved detection
include(CheckCXXCompiler)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(TestBigEndian)

# Detect Intel oneAPI compilers
if(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM" OR
	CMAKE_CXX_COMPILER_ID MATCHES "Intel" OR
	CMAKE_CXX_COMPILER MATCHES "icx" OR
	CMAKE_CXX_COMPILER MATCHES "icpc" OR
	CMAKE_CXX_COMPILER MATCHES "dpcpp")
	set(USING_INTEL_COMPILER TRUE)
	message(STATUS "Intel compiler detected: ${CMAKE_CXX_COMPILER_ID}")
endif()

# Find Intel oneAPI components
set(CMAKE_PREFIX_PATH "$ENV{ONEAPI_ROOT}/tbb;$ENV{ONEAPI_ROOT}/ipp;$ENV{ONEAPI_ROOT}/mkl;${CMAKE_PREFIX_PATH}")

find_package(TBB QUIET)

if(TBB_FOUND)
	message(STATUS "Intel TBB found: ${TBB_VERSION}")
else()
	message(STATUS "Intel TBB not found, some optimizations will be disabled")
endif()

find_package(IPP QUIET)

if(IPP_FOUND)
	message(STATUS "Intel IPP found: ${IPP_VERSION}")
else()
	message(STATUS "Intel IPP not found, some optimizations will be disabled")
endif()

find_package(MKL QUIET)

if(MKL_FOUND)
	message(STATUS "Intel MKL found: ${MKL_VERSION}")
else()
	message(STATUS "Intel MKL not found, will fallback to standard math libraries")
endif()

# oneAPI configuration options
option(USE_INTEL_COMPILER "Prefer Intel oneAPI compiler if available" ON)
option(USE_INTEL_TBB "Use Intel Threading Building Blocks" ON)
option(USE_INTEL_IPP "Use Intel Integrated Performance Primitives" ON)
option(USE_INTEL_MKL "Use Intel Math Kernel Library" ON)
option(USE_INTEL_DPCPP "Use Intel Data Parallel C++" OFF)

# --- FFmpeg Configuration ---
# Use local FFmpeg source code instead of system installation
set(FFMPEG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ffmpeg-master")

# Set required FFmpeg components
set(FFMPEG_COMPONENTS
	avutil
	avformat
	avcodec
	avfilter
	swresample
	swscale
)

# Print status message
message(STATUS "Using local FFmpeg source from: ${FFMPEG_SOURCE_DIR}")

# Create FFmpeg include paths
set(FFMPEG_INCLUDE_DIRS
	"${FFMPEG_SOURCE_DIR}"
)

# Add each component's include directory
foreach(component ${FFMPEG_COMPONENTS})
	list(APPEND FFMPEG_INCLUDE_DIRS "${FFMPEG_SOURCE_DIR}/lib${component}")
endforeach()

# Build FFmpeg libraries from source
set(FFMPEG_LIBRARIES)

# Define sources for each FFmpeg component
foreach(component ${FFMPEG_COMPONENTS})
	file(GLOB_RECURSE FFMPEG_${component}_SOURCES
		"${FFMPEG_SOURCE_DIR}/lib${component}/*.c"
	)

	# Create library target
	add_library(ffmpeg_${component} STATIC ${FFMPEG_${component}_SOURCES})

	# Add component-specific include paths and definitions
	target_include_directories(ffmpeg_${component} PRIVATE
		${FFMPEG_INCLUDE_DIRS}
	)

	# Add component to libraries list
	list(APPEND FFMPEG_LIBRARIES ffmpeg_${component})

	# Common compile definitions for all FFmpeg components
	target_compile_definitions(ffmpeg_${component} PRIVATE
		HAVE_AV_CONFIG_H
		_CRT_SECURE_NO_WARNINGS
		_CRT_NONSTDC_NO_DEPRECATE
		_WINSOCK_DEPRECATED_NO_WARNINGS
		FF_API_OLD_CHANNEL_LAYOUT=0
	)

	# Disable specific warnings for FFmpeg code
	if(MSVC)
		target_compile_options(ffmpeg_${component} PRIVATE
			/wd4244 /wd4267 /wd4018 /wd4996
		)
	endif()
endforeach()

# --- liblo Configuration ---
option(LIBLO_ENABLE_THREADS "Build liblo with threading support" ON)
option(BUILD_LO_TOOLS "Build liblo command-line tools (oscsend, oscdump)" ON)
option(BUILD_LO_EXAMPLES "Build liblo examples" ON)
option(BUILD_DOCUMENTATION "Build Doxygen documentation" ON)

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
	${FFMPEG_INCLUDE_DIRS}
)

# Source files for AudioEngine
file(GLOB AUDIO_ENGINE_SOURCES
	"src/AudioEngine/*.cpp"
	"src/AudioEngine/*.h"
)

# Define AudioEngine library
add_library(AudioEngine SHARED ${AUDIO_ENGINE_SOURCES})

# Link with oneAPI components and liblo
target_link_libraries(AudioEngine PRIVATE liblo)

# Conditionally link with available oneAPI components
if(TBB_FOUND)
	target_link_libraries(AudioEngine PRIVATE TBB::tbb)
	target_compile_definitions(AudioEngine PRIVATE HAVE_TBB)
endif()

if(IPP_FOUND)
	target_link_libraries(AudioEngine PRIVATE IPP::ipp)
	target_compile_definitions(AudioEngine PRIVATE HAVE_IPP)
endif()

if(MKL_FOUND)
	target_link_libraries(AudioEngine PRIVATE
		MKL::mkl_intel_lp64
		MKL::mkl_sequential # Or MKL::mkl_tbb_thread if using TBB threading
		MKL::mkl_core)
	target_compile_definitions(AudioEngine PRIVATE HAVE_MKL)
endif()

# Apply optimizations globally if using Intel compiler
if(USING_INTEL_COMPILER)
	# Common flags for both classic Intel compiler and Intel LLVM-based compiler
	add_compile_options(
		-O3 # High optimization level
		-xHost # Optimize for the build machine's architecture
		$<$<COMPILE_LANGUAGE:CXX>:-fp-model=fast=2> # Faster FP math for C++
		$<$<COMPILE_LANGUAGE:C>:-fp-model=fast=2> # Faster FP math for C
	)

	# LLVM-based Intel compiler (icx/icpx) specific flags
	if(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
		add_compile_options(
			-fiopenmp # Intel optimized OpenMP (for icx)
			-ffast-math # Fast math operations
			-finline-functions # Inline function expansion
			-flto # Link-time optimization
		)
		message(STATUS "Applied Intel LLVM compiler optimizations")

	# Classic Intel compiler (icc/icpc) specific flags
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
		add_compile_options(
			-qopenmp # Intel OpenMP support (for icc)
			-ipo # Interprocedural optimization
			-inline-forceinline # Force inlining for performance
			-no-prec-div # Faster but less precise division
			-qopt-mem-layout-trans=3 # Memory layout transformations
		)
		message(STATUS "Applied Classic Intel compiler optimizations")
	endif()

	# Add vectorization report for debugging performance
	option(VECTORIZATION_REPORT "Generate vectorization report" OFF)

	if(VECTORIZATION_REPORT)
		if(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
			add_compile_options(-qopt-report=5 -qopt-report-phase=vec)
		else()
			add_compile_options(-qopt-report=5 -qopt-report-phase=vec)
		endif()
	endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options(
		-O3
		-march=native
		-ftree-vectorize
		$<$<COMPILE_LANGUAGE:CXX>:-ffast-math>
	)
endif()

# Define FFmpeg-related compilation definitions
target_compile_definitions(AudioEngine PRIVATE
	HAVE_FFMPEG
	FF_API_OLD_CHANNEL_LAYOUT=0 # Use newer FFmpeg channel layout API
)

# Add FFmpeg compile-time definitions for Windows
if(WIN32)
	target_compile_definitions(AudioEngine PRIVATE
		_CRT_SECURE_NO_WARNINGS
		_CRT_NONSTDC_NO_DEPRECATE
		_WINSOCK_DEPRECATED_NO_WARNINGS
		HAVE_AV_CONFIG_H
	)
endif()

# Add custom compile flags for FFmpeg files if needed
if(MSVC)
	# Disable specific warnings from FFmpeg code
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4244 /wd4267 /wd4018")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4267 /wd4018")
endif()

# Include the FFmpeg headers in the AudioEngine target
target_include_directories(AudioEngine PRIVATE ${FFMPEG_INCLUDE_DIRS})

# Add FFmpeg dependencies to AudioEngine
target_link_libraries(AudioEngine PRIVATE ${FFMPEG_LIBRARIES})

# Add nlohmann/json library
include(FetchContent)
FetchContent_Declare(
	json
	GIT_REPOSITORY https://github.com/nlohmann/json.git
	GIT_TAG v3.11.2 # Use the latest stable version
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# Note: nlohmann_json is the target created by the FetchContent_MakeAvailable call

# Link the JSON library to AudioEngine
target_link_libraries(AudioEngine PRIVATE nlohmann_json)

# Define main executable
add_executable(AudioEngineApp src/main.cpp)
target_link_libraries(AudioEngineApp PRIVATE AudioEngine)

# --- Optional: Build Tools ---
if(BUILD_LO_TOOLS)
	# Check if tools directory exists
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/tools")
		file(GLOB TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/*.c")

		foreach(TOOL_SRC ${TOOLS_SOURCES})
			get_filename_component(TOOL_NAME ${TOOL_SRC} NAME_WE)
			add_executable(${TOOL_NAME} ${TOOL_SRC})
			target_link_libraries(${TOOL_NAME} PRIVATE liblo)
			target_include_directories(${TOOL_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lo)
		endforeach()

		# Install tools
		install(TARGETS ${TOOL_TARGETS} RUNTIME DESTINATION bin)
	else()
		message(STATUS "Tools directory not found, skipping build of command-line tools")
		set(BUILD_LO_TOOLS OFF CACHE BOOL "Build liblo command-line tools (oscsend, oscdump)" FORCE)
	endif()
endif()

# --- Optional: Build Examples ---
if(BUILD_LO_EXAMPLES)
	# Check if examples directory exists
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/examples")
		# C++ Example
		if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/examples/cpp_example.cpp")
			add_executable(cpp_example src/examples/cpp_example.cpp)
			target_link_libraries(cpp_example PRIVATE liblo)
			target_include_directories(cpp_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lo)
			list(APPEND EXAMPLE_TARGETS cpp_example)
		endif()

		# C Examples
		file(GLOB C_EXAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/src/examples/*.c")

		foreach(EXAMPLE_SRC ${C_EXAMPLES})
			get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)
			add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC})
			target_link_libraries(${EXAMPLE_NAME} PRIVATE liblo)
			target_include_directories(${EXAMPLE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lo)
			list(APPEND EXAMPLE_TARGETS ${EXAMPLE_NAME})
		endforeach()

		# Optionally install examples
		install(TARGETS ${EXAMPLE_TARGETS} RUNTIME DESTINATION bin/examples)
	else()
		message(STATUS "Examples directory not found, skipping build of examples")
		set(BUILD_LO_EXAMPLES OFF CACHE BOOL "Build liblo examples" FORCE)
	endif()
endif()

# --- Optional: Doxygen Documentation ---
find_package(Doxygen)

if(DOXYGEN_FOUND)
	if(BUILD_DOCUMENTATION)
		if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/reference.doxygen")
			set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/reference.doxygen)
			set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/doc)

			# Create doc directory if it doesn't exist
			file(MAKE_DIRECTORY ${DOXYGEN_OUT})

			# Configure Doxygen file
			configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT}/Doxyfile @ONLY)

			# Add custom target
			add_custom_target(doc
				COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}/Doxyfile
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
				COMMENT "Generating API documentation with Doxygen"
				VERBATIM)
		else()
			message(STATUS "Doxygen configuration file not found, skipping documentation build")
			set(BUILD_DOCUMENTATION OFF CACHE BOOL "Build Doxygen documentation" FORCE)
		endif()
	endif()
else()
	message(STATUS "Doxygen not found, documentation will not be built")
	set(BUILD_DOCUMENTATION OFF CACHE BOOL "Build Doxygen documentation" FORCE)
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

# Install liblo headers
install(DIRECTORY src/lo/
	DESTINATION include/lo
	FILES_MATCHING PATTERN "*.h"
)
