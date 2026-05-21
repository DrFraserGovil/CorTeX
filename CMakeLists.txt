cmake_minimum_required(VERSION 3.15)
project(cortex VERSION 0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-O3 -Wall -Wno-sign-compare)

# Define where the binary goes
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Find sources first
file(GLOB_RECURSE ALL_SOURCES "sentinel/*.cpp")
list(FILTER ALL_SOURCES EXCLUDE REGEX ".build/.*")
list(FILTER ALL_SOURCES EXCLUDE REGEX "libs/*")
list(FILTER ALL_SOURCES EXCLUDE REGEX "old/*")   ## temporary ignore the old src for some destructive changes

# Create the executable target
add_executable(cortex ${ALL_SOURCES} "${GEN_HEADER}")

# Link libraries

### For development purposes, it is quicker and easier to use the local libraru
### But other people will need to download it
set(LOCAL_JSL "${CMAKE_CURRENT_SOURCE_DIR}/../JSL")
if(EXISTS "${LOCAL_JSL}/CMakeLists.txt")
    message(STATUS "Using local JSL from ${LOCAL_JSL}")
    add_subdirectory("${LOCAL_JSL}" "${CMAKE_CURRENT_BINARY_DIR}/jsl_build")
else()
    message(STATUS "Local JSL not found. Fetching from Git...")
    include(FetchContent)
        FetchContent_Declare(
        jsl
        GIT_REPOSITORY https://github.com/DrFraserGovil/JSL.git
        GIT_TAG        deheader  
    )
    FetchContent_MakeAvailable(jsl)
endif()



set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(cortex PRIVATE Threads::Threads)
target_include_directories(cortex PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sentinel/resources")

target_link_libraries(cortex PRIVATE jsl)

