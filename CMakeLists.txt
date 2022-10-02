cmake_minimum_required(VERSION 3.0)

project(rasterizer)

file(GLOB SRC
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/include/*.hpp"
)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(/W4 /WX)

add_subdirectory(3rdparty)

add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include/)
target_link_libraries(${PROJECT_NAME} sfml-system sfml-window sfml-graphics sfml-network sfml-audio)
target_link_libraries(${PROJECT_NAME} glm)