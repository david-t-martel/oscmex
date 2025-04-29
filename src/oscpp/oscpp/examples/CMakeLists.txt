# Examples CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# List of example applications
set(EXAMPLES
    client_example
    server_example
)

# Build each example
foreach(example ${EXAMPLES})
    # Skip if source file doesn't exist
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp")
        message(STATUS "Example source file ${example}.cpp not found, skipping")
        continue()
    endif()

    # Add the executable
    add_executable(${example} ${example}.cpp)

    # Link with the OSC library
    target_link_libraries(${example} PRIVATE oscpp)

    # Set output directory for examples
    set_target_properties(${example}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # Install examples if requested
    if(OSCPP_INSTALL)
        install(TARGETS ${example}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
        )
    endif()
endforeach()

# Optional: Add example data files if needed
file(GLOB EXAMPLE_DATA "${CMAKE_CURRENT_SOURCE_DIR}/data/*")

if(EXAMPLE_DATA)
    file(COPY ${EXAMPLE_DATA} DESTINATION "${CMAKE_BINARY_DIR}/bin/data")

    if(OSCPP_INSTALL)
        install(FILES ${EXAMPLE_DATA}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/oscpp/examples
        )
    endif()
endif()
