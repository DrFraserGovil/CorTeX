cmake_minimum_required(VERSION 3.15)
project(cortex VERSION 0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-O3 -Wall -Wno-sign-compare)


# 1. Define where the binary goes
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")


# 2. Find sources first
file(GLOB_RECURSE ALL_SOURCES "sentinel/*.cpp")
list(FILTER ALL_SOURCES EXCLUDE REGEX ".build/.*")
list(FILTER ALL_SOURCES EXCLUDE REGEX "libs/*")
list(FILTER ALL_SOURCES EXCLUDE REGEX "src/*")   ## temporary ignore the old src for some destructive changes

# 3. Create the executable target
add_executable(cortex ${ALL_SOURCES} "${GEN_HEADER}")

# 4. Define the JSL interface library
add_library(jsl INTERFACE)
# target_include_directories(jsl INTERFACE 
#     ${CMAKE_CURRENT_SOURCE_DIR}/sentinel/libs
# )
target_include_directories(jsl INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(cortex PRIVATE Threads::Threads)
target_include_directories(cortex PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sentinel/resources")
target_link_libraries(cortex PRIVATE jsl)