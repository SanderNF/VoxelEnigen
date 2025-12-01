#include "block.h"

float getVOffset(BlockType type, int faceIndex) {
    const float vScale = 1.0f / 6.0f; // atlas rows

    switch(type) {
        case GRASS:
            if (faceIndex == 5) return 5 * vScale;
            else if (faceIndex == 4) return 4 * vScale;
            else return 4 * vScale;
        case DIRT: return 4 * vScale;
        case STONE: return 3 * vScale;
        case WOOD:
            if (faceIndex == 4 || faceIndex == 5) return 1 * vScale;
            else return 2 * vScale;
        case LEAVES: return 0 * vScale;
        default: return 0;
    }
}