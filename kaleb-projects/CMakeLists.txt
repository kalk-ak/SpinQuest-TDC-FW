# Version & Project Name
cmake_minimum_required(VERSION 3.23)			# Using modern cmake
project(daq_server VERSION 0.01 LANGUAGES CXX)

# Make sure that compile_comands.json is created to avoid LSP warnings
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Download all Dependencies automatically
include(FetchContent)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.6.0
)
<<<<<<< HEAD
FetchContent_MakeAvailable(cli11)



# Define the Executable
add_executable(daq_server)

# Use C++ 17
target_compile_features(daq_server PRIVATE cxx_std_17)

# Setup Includes
# "Look in 'include/' for headers, but don't force this on others unless they link to me"
target_include_directories(daq_server PRIVATE include)

# 6. Link Libraries
# "I need CLI11 to build."
target_link_libraries(daq_server PRIVATE CLI11::CLI11)
=======

# Download spdlog for better logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0   # use stable version
)

FetchContent_MakeAvailable(cli11)
FetchContent_MakeAvailable(spdlog)

# ---------------- END OF DOWNLOAD --------------------------------------

# Use C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shared sources
set(SHARED_SOURCES
  src/UniqueFD.cpp
  src/defined_utilities.cpp
)

# DAQ Server Executable
add_executable(daq_server
  src/server_main.cpp
  src/daq_server.cpp
  ${SHARED_SOURCES}
)
target_include_directories(daq_server PRIVATE include)
target_link_libraries(daq_server PRIVATE CLI11::CLI11 spdlog::spdlog)

# Fake FPGA Client Executable
add_executable(fake_fpga
  src/fpga_main.cpp
  src/fake_fpga_client.cpp
  ${SHARED_SOURCES}
)
target_include_directories(fake_fpga PRIVATE include)
target_link_libraries(fake_fpga PRIVATE CLI11::CLI11 spdlog::spdlog)
>>>>>>> feature
