# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# Set path to vcpkg toolchain file if VCPKG_ROOT is defined
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

    if(NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ${VCPKG_TOOLCHAIN_FILE})
    endif()
endif()

# First try to find GTest using vcpkg/CMake's built-in config
find_package(GTest CONFIG QUIET)

# If the CONFIG mode doesn't work, try the module mode
if(NOT GTest_FOUND)
    find_package(GTest QUIET)
endif()

# If CMake's FindGTest didn't find it, try with pkg-config as fallback
if(NOT GTest_FOUND AND NOT GTEST_FOUND)
    find_package(PkgConfig QUIET)

    if(PkgConfig_FOUND)
        pkg_check_modules(GTEST QUIET gtest)
    endif()
endif()

# If GTest still not found, download and build it
if(NOT GTest_FOUND AND NOT GTEST_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0 # Updated to a newer version that's compatible with modern CMake
    )

    # Configure gtest to use shared CRT on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Use FetchContent_MakeAvailable instead of FetchContent_Populate to avoid deprecation warning
    FetchContent_MakeAvailable(googletest)

    # Set GTest as found and set variables for backwards compatibility
    set(GTEST_FOUND TRUE)
    set(GTEST_INCLUDE_DIRS ${googletest_SOURCE_DIR}/googletest/include)
    set(GTEST_LIBRARIES GTest::gtest)
    set(GTEST_MAIN_LIBRARIES GTest::gtest_main)
endif()

# Define the test executables
set(TEST_SOURCES
    test_address.cpp
    test_bundle.cpp
    test_message.cpp
    test_pattern_matching.cpp
    test_server.cpp
    test_tcp_framing.cpp
    test_timetag.cpp
)

# If GTest was found via either method
if(GTest_FOUND OR GTEST_FOUND)
    # Include GoogleTest-specific CMake module to support the gtest_discover_tests command
    include(GoogleTest)

    # For modern CMake, use GTest namespace, for older use variables
    if(TARGET GTest::gtest)
        set(GTEST_LIBRARIES GTest::gtest)
        set(GTEST_MAIN_LIBRARIES GTest::gtest_main)
    endif()

    if(DEFINED GTEST_INCLUDE_DIRS)
        include_directories(${GTEST_INCLUDE_DIRS})
    endif()

    # Create a test executable for each test source
    foreach(test_source ${TEST_SOURCES})
        # Get the test name from the filename without extension
        get_filename_component(test_name ${test_source} NAME_WE)

        # Add executable
        add_executable(${test_name} ${test_source})

        # Link against the library and GTest
        target_link_libraries(${test_name} PRIVATE
            oscpp
            ${GTEST_LIBRARIES}
            ${GTEST_MAIN_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT} # pthread or equivalent for GTest
        )

        # Add the test to CTest
        add_test(NAME ${test_name} COMMAND ${test_name})

        # Set output directory for tests
        set_target_properties(${test_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
        )
    endforeach()

    # If tests need to find any data files
    set_tests_properties(${test_names} PROPERTIES ENVIRONMENT
        "OSC_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/test_data"
    )

    message(STATUS "GTest found - building tests")
else()
    # Fallback to a simple test framework if GTest isn't available
    message(WARNING "GTest not found - building simplified tests without framework")

    # Create a simple test runner for each test
    foreach(test_source ${TEST_SOURCES})
        # Get the test name from the filename without extension
        get_filename_component(test_name ${test_source} NAME_WE)

        # Add executable with custom main function
        add_executable(${test_name} ${test_source})
        target_compile_definitions(${test_name} PRIVATE SIMPLE_TEST_FRAMEWORK)

        # Link against the library
        target_link_libraries(${test_name} PRIVATE oscpp)

        # Add the test to CTest
        add_test(NAME ${test_name} COMMAND ${test_name})

        # Set output directory for tests
        set_target_properties(${test_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
        )
    endforeach()
endif()
