# Version & Project Name
cmake_minimum_required(VERSION 3.14)
project(daq_server VERSION 0.01 LANGUAGES CXX)

# Download all Dependencies automatically
include(FetchContent)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(cli11)



# Define the Executable (The Target)
add_executable(daq_server 
	#TODO: Add executables here
)

# Use C++ 17
target_compile_features(daq_server PRIVATE cxx_std_17)

# Setup Includes
# "Look in 'include/' for headers, but don't force this on others unless they link to me"
target_include_directories(daq_server PRIVATE include)

# 6. Link Libraries
# "I need CLI11 to build."
target_link_libraries(daq_server PRIVATE CLI11::CLI11)
