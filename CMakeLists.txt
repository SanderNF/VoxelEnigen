
cmake_minimum_required(VERSION 3.5)
project(OpenGLExample)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

set(GLFW3_LIB_DIR "C:/msys64/mingw64/lib")
set(GLFW3_INCLUDE_DIR "C:/msys64/mingw64/include")

include_directories(${GLFW3_INCLUDE_DIR})
link_directories(${GLFW3_LIB_DIR})

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/src/imgui")
include_directories(${IMGUI_DIR})
include_directories(${IMGUI_DIR}/backends)

file(GLOB IMGUI_SOURCES
        "${IMGUI_DIR}/*.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

add_executable(app
        src/main.cpp
        src/camera.cpp
        src/renderer.cpp
        src/chunk.cpp
        src/block.cpp
        src/world.cpp
        src/player.cpp
        src/texture_atlas.cpp
        ${IMGUI_SOURCES}
)

target_link_libraries(app
        OpenGL32
        glew32
        glfw3
)
