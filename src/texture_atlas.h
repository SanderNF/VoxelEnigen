#pragma once
#include "block.h"
#include <unordered_map>
#include <string>

struct AtlasConfig {
    int columns = 1;
    int rows = 16;

    float getUScale() const { return 1.0f / columns; }
    float getVScale() const { return 1.0f / rows; }
};

struct AtlasTexture {
    int column;
    int row;

    AtlasTexture(int col = 0, int r = 0) : column(col), row(r) {}
};

class TextureAtlas {
public:
    TextureAtlas();

    AtlasTexture getTexture(BlockType type, int faceIndex) const;

    float getUOffset(const AtlasTexture& tex) const;
    float getVOffset(const AtlasTexture& tex) const;

    const AtlasConfig& getConfig() const { return config; }

private:
    AtlasConfig config;

    std::unordered_map<BlockType, AtlasTexture[6]> blockTextures;

    void initializeTextures();
};

extern TextureAtlas g_textureAtlas;