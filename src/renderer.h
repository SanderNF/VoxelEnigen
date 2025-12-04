#pragma once
#include <GL/glew.h>
#include <string>

class Renderer {
public:
    unsigned int shaderProgram;
    GLuint atlasTexture;        // albedo (color) atlas
    GLuint normalAtlasTexture;  // normal map atlas (tangent-space normals)
    GLuint specularAtlasTexture;// specular/roughness atlas (R=specular intensity, G=unused, B=unused, A=roughness)
    GLuint lightMapTexture;     // placeholder / future dynamic light map

    Renderer();
    ~Renderer();

    bool initialize();
    unsigned int getShaderProgram() const { return shaderProgram; }
    GLuint getAtlasTexture() const { return atlasTexture; }
    GLuint getNormalAtlasTexture() const { return normalAtlasTexture; }
    GLuint getSpecularAtlasTexture() const { return specularAtlasTexture; }
    GLuint getLightMapTexture() const { return lightMapTexture; }

private:
    unsigned int compileShader(unsigned int type, const char* src);
    unsigned int createShaderProgram();
    GLuint loadTexture(const char* path, bool hasAlpha = true, bool isNormalMap = false);
};