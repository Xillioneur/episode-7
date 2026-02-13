cmake_minimum_required(VERSION 3.20)
project(NeonNexus VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add optimization flags
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3)
endif()

# Use pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_executable(neon_nexus main.cpp)

# Include directories
target_include_directories(neon_nexus PRIVATE 
    ${SDL2_INCLUDE_DIRS} 
    ${SDL2_TTF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Use link_libraries with full paths
target_link_libraries(neon_nexus PRIVATE 
    ${SDL2_LINK_LIBRARIES} 
    ${SDL2_TTF_LINK_LIBRARIES}
)

# Handle thread support
find_package(Threads REQUIRED)
target_link_libraries(neon_nexus PRIVATE Threads::Threads)