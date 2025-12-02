#include "texture_atlas.h"

TextureAtlas g_textureAtlas;

TextureAtlas::TextureAtlas() {
    initializeTextures();
}

void TextureAtlas::initializeTextures() {

    blockTextures[GRASS][0] = AtlasTexture(0, 5);
    blockTextures[GRASS][1] = AtlasTexture(0, 5);
    blockTextures[GRASS][2] = AtlasTexture(0, 5);
    blockTextures[GRASS][3] = AtlasTexture(0, 5);
    blockTextures[GRASS][4] = AtlasTexture(0, 5);
    blockTextures[GRASS][5] = AtlasTexture(0, 6);

    for (int i = 0; i < 6; i++) {
        blockTextures[DIRT][i] = AtlasTexture(0, 5);
    }

    for (int i = 0; i < 6; i++) {
        blockTextures[STONE][i] = AtlasTexture(0, 4);
    }

    for (int i = 0; i < 6; i++) {
        blockTextures[SNOW][i] = AtlasTexture(0, 0);
    }

    blockTextures[WOOD][0] = AtlasTexture(0, 3);
    blockTextures[WOOD][1] = AtlasTexture(0, 3);
    blockTextures[WOOD][2] = AtlasTexture(0, 3);
    blockTextures[WOOD][3] = AtlasTexture(0, 3);
    blockTextures[WOOD][4] = AtlasTexture(0, 2);
    blockTextures[WOOD][5] = AtlasTexture(0, 2);



    for (int i = 0; i < 6; i++) {
        blockTextures[LEAVES][i] = AtlasTexture(0, 1);
    }

    for (int i = 0; i < 6; i++) {
        blockTextures[AIR][i] = AtlasTexture(0, 0);
    }
}

AtlasTexture TextureAtlas::getTexture(BlockType type, int faceIndex) const {
    auto it = blockTextures.find(type);
    if (it != blockTextures.end()) {
        return it->second[faceIndex];
    }
    return AtlasTexture(0, 0);
}

float TextureAtlas::getUOffset(const AtlasTexture& tex) const {
    return tex.column * config.getUScale();
}

float TextureAtlas::getVOffset(const AtlasTexture& tex) const {
    return tex.row * config.getVScale();
}