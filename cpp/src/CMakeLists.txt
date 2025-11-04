# Create your own CMakeLists.txt file here that will build your project, based 
# on the directory structure you choose. You can use the CMakeLists.txt files
# we provided in the src directory of Project 1 and Project 2 as a starting point.

# You will also have to add a CMakelists.txt file in the common/ directory. 

# Running the standard CMake build sequence should result in the following executables:
#   wSender
#   wReceiver
#   wSenderOpt
#   wReceiverOpt


cmake_minimum_required(VERSION 3.28)
project(eecs489-project3-wn25)

# ---- Executable ----
add_executable(iperf
  wSender.cpp
  wReceiver.cpp
  wSenderOpt.cpp
  wReceiverOpt.cpp
)

target_compile_features(iperf PRIVATE cxx_std_17)


find_package(cxxopts CONFIG REQUIRED)
if(TARGET cxxopts::cxxopts)
  target_link_libraries(iperf PRIVATE cxxopts::cxxopts)
elseif(TARGET cxxopts)
  target_link_libraries(iperf PRIVATE cxxopts)
else()
  message(FATAL_ERROR "cxxopts target not found even though the package was located")
endif()

include(FetchContent)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)
target_link_libraries(iperf PRIVATE spdlog::spdlog)   