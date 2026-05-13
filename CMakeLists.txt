cmake_minimum_required(VERSION 3.20)
project(OS_Project C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.0
        GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(dijkstra main_dijkstra.c graph.c)
add_executable(sim main_sim.c graph.c gui.c)

target_link_libraries(sim PRIVATE raylib)
