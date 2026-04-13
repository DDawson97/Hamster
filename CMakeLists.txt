cmake_minimum_required(VERSION 3.10)
project(GDContest2026)

set(CMAKE_CXX_STANDARD 17) 

find_package(nlohmann_json REQUIRED)

find_package(OGDF REQUIRED)

add_executable(solver simulated_annealing.cpp)

# Link the library
target_link_libraries(solver PRIVATE 
    nlohmann_json
    OGDF
)

# BULLETPROOFING: Explicitly tell CMake where the headers are.
# Based on your previous logs, your OGDF source is in /Users/bshen/ogdf
# target_include_directories(my-exe PRIVATE 
#     "/Users/bshen/ogdf/include"
#     "/Users/bshen/ogdf/include/ogdf-release"
# )