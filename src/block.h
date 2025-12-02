#pragma once

enum BlockType {
    AIR = 0,
    GRASS,
    DIRT,
    STONE,
    WOOD,
    LEAVES,
    SNOW
};

enum class LogAxis {
    Y = 0,
    X = 1,
    Z = 2
};

struct Block {
    BlockType type = AIR;
    LogAxis axis = LogAxis::Y;
};

float getVOffset(BlockType type, int faceIndex);