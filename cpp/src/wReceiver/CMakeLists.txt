# Set the WRECEIVER_SOURCES variable to the list of all source files in the current directory
set(
    WRECEIVER_SOURCES 
    wReceiver.cpp
)

# Tell CMake to create an executable named 'loadBalancer' from the source files
add_executable(wReceiver ${WRECEIVER_SOURCES})

# Ensure that the cxxopts and common libraries are linked to the loadBalancer executable
target_link_libraries(wReceiver PRIVATE cxxopts::cxxopts common spdlog::spdlog )

# Include the common directory for headers (e.g. LoadBalancerProtocol.h)
target_include_directories(wReceiver PRIVATE ${PROJECT_SOURCE_DIR}/common)