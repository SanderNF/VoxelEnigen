#pragma once
#include <GL/glew.h>

class Renderer {
public:
    unsigned int shaderProgram;
    GLuint atlasTexture;

    Renderer();
    ~Renderer();

    bool initialize();
    unsigned int getShaderProgram() const { return shaderProgram; }
    GLuint getAtlasTexture() const { return atlasTexture; }

private:
    unsigned int compileShader(unsigned int type, const char* src);
    unsigned int createShaderProgram();
    GLuint loadTexture(const char* path);
};