#include "block.h"
#include "texture_atlas.h"

float getVOffset(BlockType type, int faceIndex) {
    AtlasTexture tex = g_textureAtlas.getTexture(type, faceIndex);
    return g_textureAtlas.getVOffset(tex);
}