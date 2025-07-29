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
)

set(HEADERS
    src/WindowManager.h
    src/TabSwitcher.h
    src/Utils.h
)

# Create executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

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
    )
endif()

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "tabswitcher"
)

# Compiler specific settings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif() 