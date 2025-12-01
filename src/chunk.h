#pragma once
#include "block.h"
#include <vector>
#include <GL/glew.h>

struct ChunkManager;

extern float cubeFaces[6][30];

struct Chunk {
    unsigned int width = 16;
    unsigned int depth = 16;
    unsigned int height = 128;
    std::vector<Block> blocks;
    int chunkX, chunkZ;

    Chunk(int cx = 0, int cz = 0, unsigned int w = 16, unsigned int d = 16, unsigned int h = 128);

    Block& getBlock(int x, int y, int z);
    void setBlock(int x, int y, int z, BlockType type);
};

struct ChunkMesh {
    std::vector<float> vertices;
    unsigned int VAO = 0, VBO = 0;

    void generateMesh(Chunk& chunk, ChunkManager* manager);
    void appendFaceWithAtlas(float face[30], int x, int y, int z, int chunkX, int chunkZ,
                            int chunkWidth, int chunkDepth, Block& block, int faceIndex);
    void uploadToGPU();
    void draw();
};

struct ManagedChunk {
    Chunk chunk;
    ChunkMesh mesh;

    bool terrainGenerated = false;
    bool structuresGenerated = false;
    bool meshUploaded = false;
    bool meshDirty = true;

    ManagedChunk(int cx, int cz);
};