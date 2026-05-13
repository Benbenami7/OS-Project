cmake_minimum_required(VERSION 3.20)
project(OS_Project C)

set(CMAKE_C_STANDARD 11)

# 1. מורה ל-CMake להוריד את Raylib הרשמי ישירות מגיטהאב
include(FetchContent)
FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.0 # גרסה יציבה
        GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(raylib)

# 2. מגדיר את קובץ ההרצה שלנו
add_executable(OS_Project main.c graph.c gui.c)

# 3. מחבר את Raylib לפרויקט שלנו בצורה חלקה
target_link_libraries(OS_Project PRIVATE raylib)