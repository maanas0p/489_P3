# Set the LOADBALANCER_SOURCES variable to the list of all source files in the current directory
set(
    WSENDER_SOURCES 
    wSender.cpp
)

# Tell CMake to create an executable named 'loadBalancer' from the source files
add_executable(wSender ${WSENDER_SOURCES})

# Ensure that the cxxopts and common libraries are linked to the loadBalancer executable
target_link_libraries(wSender PRIVATE cxxopts::cxxopts common spdlog::spdlog)

# Include the common directory for headers (e.g. LoadBalancerProtocol.h)
target_include_directories(wSender PRIVATE ${PROJECT_SOURCE_DIR}/common)

