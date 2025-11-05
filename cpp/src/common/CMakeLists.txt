# common/CMakeLists.txt
add_library(common INTERFACE)

# Ensure the headers in common/ are accessible
target_include_directories(common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

