#include "renderer.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stbimage/stb_image.h"
#include <iostream>


const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aUV;
    Normal = mat3(transpose(inverse(model))) * aNormal; // transform normal to world space
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;

uniform sampler2D tex0;
uniform vec3 lightDir;       // set from C++ once
uniform vec3 lightColor;     // set from C++
uniform vec3 ambientColor;   // set from C++

void main() {
    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, -lightDir), 0.0);

    vec3 texColor = texture(tex0, TexCoord).rgb;

    // combine diffuse + ambient
    vec3 result = ambientColor * texColor + diff * lightColor * texColor;

    FragColor = vec4(result, 1.0);
}
)";


Renderer::Renderer() : shaderProgram(0), atlasTexture(0) {
}

Renderer::~Renderer() {
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (atlasTexture) glDeleteTextures(1, &atlasTexture);
}

bool Renderer::initialize() {
    shaderProgram = createShaderProgram();
    if (!shaderProgram) return false;

    atlasTexture = loadTexture("../src/textures/atlas.png");
    if (!atlasTexture) return false;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // --- Set fixed sun ---
    glUseProgram(shaderProgram);

    GLint lightDirLoc     = glGetUniformLocation(shaderProgram, "lightDir");
    GLint lightColorLoc   = glGetUniformLocation(shaderProgram, "lightColor");
    GLint ambientColorLoc = glGetUniformLocation(shaderProgram, "ambientColor");

    // Fixed sun direction (from sun to scene)
    float sunDir[3] = { 0.5f, 1.0f, 0.3f };
    float len = std::sqrt(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    sunDir[0] /= len;
    sunDir[1] /= len;
    sunDir[2] /= len;
    glUniform3f(lightDirLoc, sunDir[0], sunDir[1], sunDir[2]);

    // Light colors
    glUniform3f(lightColorLoc, 1.0f, 1.0f, 1.0f);      // white sunlight
    glUniform3f(ambientColorLoc, 0.4f, 0.4f, 0.5f);    // soft ambient for blocks and sky

    return true;
}



unsigned int Renderer::compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "Shader Error: " << infoLog << std::endl;
    }

    return shader;
}

unsigned int Renderer::createShaderProgram() {
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cout << "Linker Error: " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLuint Renderer::loadTexture(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);

    if (!data) {
        std::cout << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLenum format;
    if (channels == 1)      format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    stbi_image_free(data);
    return textureID;
}