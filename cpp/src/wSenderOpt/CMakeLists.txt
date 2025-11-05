# Set the LOADBALANCER_SOURCES variable to the list of all source files in the current directory
set(
    WSENDEROPT_SOURCES 
    wSenderOpt.cpp
)

# Tell CMake to create an executable named 'loadBalancer' from the source files
add_executable(wSenderOpt ${WSENDEROPT_SOURCES})

# Ensure that the cxxopts and common libraries are linked to the loadBalancer executable
target_link_libraries(wSenderOpt PRIVATE cxxopts::cxxopts common spdlog::spdlog)

# Include the common directory for headers (e.g. LoadBalancerProtocol.h)
target_include_directories(wSenderOpt PRIVATE ${PROJECT_SOURCE_DIR}/common)

