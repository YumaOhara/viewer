#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

#include <glad/glad.h>
#include <SDL.h>
#include <tiffio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- プロトタイプ宣言 ---
GLuint compileShader(const char* shaderPath, GLenum type);
GLuint createShaderProgram(const char* vsPath, const char* fsPath);
uint16_t* loadTiff(const char* path, int& width, int& height, int& channels);

// --- グローバル変数 ---
float g_minVal = 0.0f;
float g_maxVal = 1.0f;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_image>" << std::endl;
        return 1;
    }

    // --- 1. 画像読み込み ---
    int width, height, channels;
    uint16_t* image_data = nullptr;

    std::filesystem::path imagePath(argv[1]);
    std::string extension = imagePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == ".tif" || extension == ".tiff") {
        std::cout << "Attempting to load TIFF image with libtiff..." << std::endl;
        image_data = loadTiff(argv[1], width, height, channels);
    } else {
        std::cout << "Attempting to load image with stb_image..." << std::endl;
        image_data = stbi_load_16(argv[1], &width, &height, &channels, 0);
    }

    if (!image_data) {
        std::cerr << "Failed to load image: " << argv[1] << std::endl;
        if (stbi_failure_reason()) std::cerr << "Reason: " << stbi_failure_reason() << std::endl;
        return 1;
    }

    // 画像データから最小輝度と最大輝度をスキャンする
    uint16_t min_pixel_val = 65535;
    uint16_t max_pixel_val = 0;
    long long total_pixels = (long long)width * height * channels;
    for (long long i = 0; i < total_pixels; ++i) {
        if (image_data[i] < min_pixel_val) min_pixel_val = image_data[i];
        if (image_data[i] > max_pixel_val) max_pixel_val = image_data[i];
    }
    std::cout << "Image Min/Max Value: " << min_pixel_val << " / " << max_pixel_val << std::endl;

    // スキャンした値を正規化して、g_minVal と g_maxVal の初期値に設定
    g_minVal = static_cast<float>(min_pixel_val) / 65535.0f;
    g_maxVal = static_cast<float>(max_pixel_val) / 65535.0f;
    
    // 最大と最小が同じ値（真っ黒や真っ白の画像）の場合の0除算を避ける
    if (g_maxVal - g_minVal < 1e-6) {
        g_maxVal = g_minVal + 0.1f; // 適当な範囲を持たせる
    }
    std::cout << "Initial Display Range (0.0-1.0): " << g_minVal << " / " << g_maxVal << std::endl;

    std::cout << "Loaded: " << argv[1] << " (" << width << "x" << height << ", " << channels << " channels, 16-bit)\n";
    std::cout << "Controls:\n - Mouse Drag L/R: Adjust Contrast\n - Mouse Drag U/D: Adjust Brightness\n";
    
    // --- 2. SDLとOpenGLの初期化 ---
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_Window* window = SDL_CreateWindow("16-bit OpenGL Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glViewport(0, 0, 800, 600);

    // --- 3. シェーダーの準備 ---
    GLuint shaderProgram = createShaderProgram("shaders/image.vert", "shaders/image.frag");

    // --- 4. 頂点データ ---
    float vertices[] = {
        // positions(x,y,z)  // texture coords(u,v)
         1.0f,  1.0f, 0.0f,   1.0f, 1.0f,
         1.0f, -1.0f, 0.0f,   1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,   0.0f, 1.0f
    };
    unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };
    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // --- 5. テクスチャの作成とアップロード ---
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum internalFormat, dataFormat;
    if (channels == 1)      { internalFormat = GL_R16;   dataFormat = GL_RED;  }
    else if (channels == 3) { internalFormat = GL_RGB16; dataFormat = GL_RGB;  }
    else if (channels == 4) { internalFormat = GL_RGBA16;dataFormat = GL_RGBA; }
    else {
        std::cerr << "Unsupported channel count: " << channels << std::endl;
        if (extension == ".tif" || extension == ".tiff") delete[] image_data;
        else stbi_image_free(image_data);
        return -1;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_SHORT, image_data);
    
    if (extension == ".tif" || extension == ".tiff") delete[] image_data;
    else stbi_image_free(image_data);
    image_data = nullptr;

    // --- 6. メインループ ---
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                glViewport(0, 0, e.window.data1, e.window.data2);
            }
            if (e.type == SDL_MOUSEMOTION && e.motion.state & SDL_BUTTON_LMASK) {
                float dx = e.motion.xrel / 400.0f;
                float dy = e.motion.yrel / 400.0f;
                float range = g_maxVal - g_minVal;
                range *= (1.0f - dx);
                range = std::max(0.001f, std::min(2.0f, range));
                g_minVal -= dy;
                g_maxVal = g_minVal + range;
                g_minVal = std::max(-1.0f, std::min(2.0f, g_minVal));
                g_maxVal = std::max(-1.0f, std::min(2.0f, g_maxVal));
            }
        }

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniform1f(glGetUniformLocation(shaderProgram, "u_minVal"), g_minVal);
        glUniform1f(glGetUniformLocation(shaderProgram, "u_maxVal"), g_maxVal);
        glUniform1i(glGetUniformLocation(shaderProgram, "u_channels"), channels);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        SDL_GL_SwapWindow(window);
    }

    // --- 7. 後片付け ---
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1, &textureID);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}

// --- libtiffで画像を読み込む関数の実装 ---
uint16_t* loadTiff(const char* path, int& width, int& height, int& channels) {
    TIFF* tif = TIFFOpen(path, "r");
    if (!tif) {
        std::cerr << "LIBTIFF Error: Cannot open " << path << std::endl;
        return nullptr;
    }

    uint32_t w, h;
    uint16_t bitsPerSample, samplesPerPixel;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    
    width = w;
    height = h;
    channels = samplesPerPixel;

    if (bitsPerSample != 16) {
        std::cerr << "LIBTIFF Error: Only 16-bit TIFF files are supported. This file is " << bitsPerSample << "-bit." << std::endl;
        TIFFClose(tif);
        return nullptr;
    }

    uint16_t* buffer = new (std::nothrow) uint16_t[(size_t)w * h * samplesPerPixel];
    if (!buffer) {
        std::cerr << "LIBTIFF Error: Failed to allocate memory for image." << std::endl;
        TIFFClose(tif);
        return nullptr;
    }

    if (TIFFIsTiled(tif)) {
        uint32_t tileWidth, tileLength;
        TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
        TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileLength);
        uint16_t* tileBuffer = (uint16_t*)_TIFFmalloc(TIFFTileSize(tif));
        if (tileBuffer) {
            for (uint32_t y = 0; y < h; y += tileLength) {
                for (uint32_t x = 0; x < w; x += tileWidth) {
                    TIFFReadTile(tif, tileBuffer, x, y, 0, 0);
                    uint32_t readWidth = std::min(tileWidth, w - x);
                    uint32_t readLength = std::min(tileLength, h - y);
                    for (uint32_t ty = 0; ty < readLength; ++ty) {
                        memcpy(buffer + ((y + ty) * w + x) * channels,
                               tileBuffer + (ty * tileWidth) * channels,
                               readWidth * channels * sizeof(uint16_t));
                    }
                }
            }
            _TIFFfree(tileBuffer);
        }
    } else {
        tsize_t scanlineSize = TIFFScanlineSize(tif);
        for (uint32_t row = 0; row < h; ++row) {
            TIFFReadScanline(tif, (char*)buffer + row * scanlineSize, row);
        }
    }

    TIFFClose(tif);
    return buffer;
}

// --- シェーダーヘルパー関数の実装 ---
GLuint compileShader(const char* shaderPath, GLenum type) {
    GLuint shader = glCreateShader(type);
    std::ifstream file(shaderPath, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "ERROR::SHADER::FILE_NOT_FOUND: " << shaderPath << std::endl;
        return 0;
    }
    std::stringstream stream;
    stream << file.rdbuf();
    file.close();
    std::string code = stream.str();
    
    const char* source = code.c_str();
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED: " << shaderPath << "\n" << infoLog << std::endl;
        return 0;
    }
    return shader;
}

GLuint createShaderProgram(const char* vsPath, const char* fsPath) {
    GLuint vs = compileShader(vsPath, GL_VERTEX_SHADER);
    GLuint fs = compileShader(fsPath, GL_FRAGMENT_SHADER);
    if (vs == 0 || fs == 0) return 0;
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}
