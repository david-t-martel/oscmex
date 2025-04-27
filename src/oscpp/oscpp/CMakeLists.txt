# CMakeLists.txt for the oscpp Library

cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(oscpp VERSION 1.0.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Source files
file(GLOB SOURCES
    src/*.cpp
)

# Create the library
add_library(oscpp ${SOURCES})

# Specify the include directories for the library
target_include_directories(oscpp PUBLIC include)

# Add examples
add_executable(client_example examples/client_example.cpp)
target_link_libraries(client_example oscpp)

add_executable(server_example examples/server_example.cpp)
target_link_libraries(server_example oscpp)

# Add tests
enable_testing()
add_subdirectory(tests)

# Installation rules
install(TARGETS oscpp
    EXPORT oscppTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/osc
    DESTINATION include
)

# Export the library
install(EXPORT oscppTargets
    FILE oscppTargets.cmake
    NAMESPACE osc::
    DESTINATION lib/cmake/oscpp
)

# Configure the package
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/oscppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/oscppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/oscppConfig.cmake"
    INSTALL_DESTINATION "lib/cmake/oscpp"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/oscppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/oscppConfigVersion.cmake"
    DESTINATION "lib/cmake/oscpp"
)