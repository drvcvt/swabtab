cmake_minimum_required(VERSION 3.16)
project(TabSwitcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_SYSTEM_VERSION 10.0)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/WindowManager.cpp
    src/TabSwitcher.cpp
    src/Utils.cpp
    src/Config.cpp
)

set(HEADERS
    src/WindowManager.h
    src/TabSwitcher.h
    src/Utils.h
    src/Config.h
)

# Add FetchContent module
include(FetchContent)

# Download and configure rapidfuzz
FetchContent_Declare(
    rapidfuzz
    GIT_REPOSITORY https://github.com/rapidfuzz/rapidfuzz-cpp.git
    GIT_TAG v3.3.2 # Using a recent stable tag
)
FetchContent_MakeAvailable(rapidfuzz)

# Create executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Define NOMINMAX to prevent conflicts with Windows' min/max macros
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)

# Link Windows libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        user32
        gdi32
        kernel32
        shell32
        ole32
        comctl32
        dwmapi
        rapidfuzz::rapidfuzz
    )
endif()

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "tabswitcher"
)

# Compiler specific settings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /FS)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif() 