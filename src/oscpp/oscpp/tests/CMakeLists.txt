# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# Try to find GTest with CMake's built-in module
find_package(GTest QUIET)

# If CMake's FindGTest didn't find it, try with pkg-config as fallback
if(NOT GTEST_FOUND)
    find_package(PkgConfig QUIET)

    if(PkgConfig_FOUND)
        pkg_check_modules(GTEST QUIET gtest)
    endif()
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
if(GTEST_FOUND)
    include_directories(${GTEST_INCLUDE_DIRS})

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
