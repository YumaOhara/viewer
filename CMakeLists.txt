cmake_minimum_required(VERSION 3.10)
project(Image16bitViewerGL)

cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE "GLVND")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2, OpenGL, そしてTIFFライブラリを探す
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(TIFF REQUIRED)

# --- 変更箇所 ---
# インクルードディレクトリの指定
# GLADと、古い形式のTIFFの両方に対応
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TIFF_INCLUDE_DIRS} # <- 古い形式のTIFFインクルードパスを追加
)

add_executable(viewer main.cpp glad.c)

# --- 変更箇所 ---
# 必要なライブラリをリンクする
# モダンなTIFF::tiffが存在すればそれを使う。なければ古い形式の変数を使う。
target_link_libraries(viewer PRIVATE 
    SDL2::SDL2 
    OpenGL::GL 
    ${TIFF_LIBRARIES} # <- 古い形式のTIFFライブラリ変数を指定
)

# shadersディレクトリをビルドディレクトリにコピーする設定
file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})
