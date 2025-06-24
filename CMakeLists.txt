cmake_minimum_required(VERSION 3.10)
project(Image16bitViewerGL)

cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE "GLVND")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(TIFF REQUIRED)

set(COMMON_LIBS PRIVATE
    SDL2::SDL2
    OpenGL::GL
    ${TIFF_LIBRARIES}
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TIFF_INCLUDE_DIRS}
)

add_executable(viewer src/main.cpp src/glad.c)

target_link_libraries(viewer ${COMMON_LIBS})

# shadersディレクトリをビルドディレクトリにコピーする設定
file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})
