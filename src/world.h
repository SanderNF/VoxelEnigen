#pragma once
#include "chunk.h"
#include <unordered_map>
#include <set>
#include <glm/glm.hpp>

struct pair_hash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

enum BiomeType {
    PLAINS = 0,
    FOREST
};

struct ChunkManager {
    std::unordered_map<std::pair<int,int>, ManagedChunk*, pair_hash> chunks;

    ManagedChunk* getChunk(int cx, int cz);
    void addChunk(int cx, int cz, ManagedChunk* chunk);
    void removeChunk(int cx, int cz);
    std::vector<ManagedChunk*> getNeighbors4(int cx, int cz);

    ~ChunkManager();
};

// Terrain generation
void initPerlin();
float perlin(float x, float y);
float getTerrainHeight(int worldX, int worldZ);
BiomeType getBiome(int worldX, int worldZ);
void generateTerrainForChunk(Chunk& chunk);
void generateTrees(Chunk& chunk, ChunkManager* manager);

// World utilities
int getChunkCoord(float worldPos);
void setBlockWorld(ChunkManager* manager, int worldX, int y, int worldZ, BlockType type,
                   LogAxis axis = LogAxis::Y, std::set<std::pair<int,int>>* modified = nullptr);
void updateChunks(ChunkManager& manager, glm::vec3 pos, int radius, unsigned int shader);