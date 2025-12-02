#include "world.h"
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

// Async
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = std::thread::hardware_concurrency()) {
        if (threadCount == 0) threadCount = 1;
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back([this]{ this->workerLoop(); });
        }
    }
    ~ThreadPool() {
        stop.store(true);
        cv.notify_all();
        for (auto& t : workers) if (t.joinable()) t.join();
    }
    void enqueue(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            jobs.push(std::move(job));
        }
        cv.notify_one();
    }
private:
    void workerLoop() {
        while (!stop.load()) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]{ return stop.load() || !jobs.empty(); });
                if (stop.load()) break;
                job = std::move(jobs.front());
                jobs.pop();
            }
            if (job) job();
        }
    }
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
};

void CompletedMeshQueue::push(CompletedMesh m) {
    std::lock_guard<std::mutex> lock(mtx);
    q.push(std::move(m));
}

bool CompletedMeshQueue::try_pop(CompletedMesh& out) {
    std::lock_guard<std::mutex> lock(mtx);
    if (q.empty()) return false;
    out = std::move(q.front());
    q.pop();
    return true;
}

struct CompletedTerrain {
    int cx;
    int cz;
};

class CompletedTerrainQueue {
public:
    void push(CompletedTerrain t) {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(t);
    }
    bool try_pop(CompletedTerrain& out) {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty()) return false;
        out = q.front();
        q.pop();
        return true;
    }
private:
    std::mutex mtx;
    std::queue<CompletedTerrain> q;
};

CompletedTerrainQueue g_completedTerrain;

static ThreadPool* g_pool = nullptr;
ThreadPool& getThreadPool() {
    if (!g_pool) {
        size_t n = std::thread::hardware_concurrency();
        size_t threads = (n > 1) ? (n - 1) : 1;
        g_pool = new ThreadPool(threads);
    }
    return *g_pool;
}

CompletedMeshQueue g_completedMeshes;

int perm[512];

void initPerlin(unsigned int seed) {
    std::srand(seed);
    std::vector<int> p(256);
    for (int i = 0; i < 256; i++) p[i] = i;

    for (int i = 255; i > 0; i--) {
        int j = std::rand() % (i + 1);
        std::swap(p[i], p[j]);
    }

    for (int i = 0; i < 512; i++) perm[i] = p[i & 255];
}

float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static inline float clampf(float x, float a, float b) {
    return std::max(a, std::min(x, b));
}

static inline float smoothstepf(float edge0, float edge1, float x) {
    float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float grad(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f*v : 2.0f*v);
}

float perlin(float x, float y) {
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;

    x -= floor(x);
    y -= floor(y);

    float u = fade(x);
    float v = fade(y);

    int aa = perm[X + perm[Y]];
    int ab = perm[X + perm[Y + 1]];
    int ba = perm[X + 1 + perm[Y]];
    int bb = perm[X + 1 + perm[Y + 1]];

    float res = lerp(
        lerp(grad(aa, x, y), grad(ba, x - 1, y), u),
        lerp(grad(ab, x, y - 1), grad(bb, x - 1, y - 1), u),
        v
    );

    return res;
}

float getTerrainHeight(int worldX, int worldZ) {
    float scale = 0.05f;
    float amplitude = 10.0f;
    float baseHeight = 50.0f;
    float n = perlin(worldX * scale, worldZ * scale);
    n = (n + 1.0f) / 2.0f;
    return baseHeight + n * amplitude;
}

BiomeType getBiome(int worldX, int worldZ) {
    float scale = 0.0015f;
    float n = perlin(worldX * scale + 500, worldZ * scale + 500);
    n = (n + 1.0f) / 2.0f;

    return (n < 0.5f) ? PLAINS : FOREST;
}

int getChunkCoord(float worldPos) {
    return (int)std::floor(worldPos / 16.0f);
}

ManagedChunk* ChunkManager::getChunk(int cx, int cz) {
    auto key = std::make_pair(cx, cz);
    auto it = chunks.find(key);
    if(it == chunks.end()) return nullptr;
    return it->second;
}

void ChunkManager::addChunk(int cx, int cz, ManagedChunk* chunk) {
    chunks[{cx, cz}] = chunk;
}

void ChunkManager::removeChunk(int cx, int cz) {
    auto key = std::make_pair(cx, cz);
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }
}

std::vector<ManagedChunk*> ChunkManager::getNeighbors4(int cx, int cz) {
    std::vector<ManagedChunk*> out;
    if (auto c = getChunk(cx+1, cz)) out.push_back(c);
    if (auto c = getChunk(cx-1, cz)) out.push_back(c);
    if (auto c = getChunk(cx, cz+1)) out.push_back(c);
    if (auto c = getChunk(cx, cz-1)) out.push_back(c);
    return out;
}

ChunkManager::~ChunkManager() {
    for(auto& pair : chunks) {
        delete pair.second;
    }
}

void setBlockWorld(ChunkManager* manager, int worldX, int y, int worldZ, BlockType type,
                   LogAxis axis, std::set<std::pair<int,int>>* modified) {
    int cx = getChunkCoord((float)worldX);
    int cz = getChunkCoord((float)worldZ);
    ManagedChunk* mc = manager->getChunk(cx, cz);
    if (!mc) return;

    int localX = worldX - cx * (int)mc->chunk.width;
    int localZ = worldZ - cz * (int)mc->chunk.depth;

    if (localX < 0 || localX >= (int)mc->chunk.width || localZ < 0 || localZ >= (int)mc->chunk.depth) return;
    if (y < 0 || y >= (int)mc->chunk.height) return;

    mc->chunk.getBlock(localX, y, localZ).type = type;
    mc->chunk.getBlock(localX, y, localZ).axis = axis;

    mc->meshDirty = true;
    if (modified) modified->insert({cx, cz});
}

void generateTerrainForChunk(Chunk& chunk) {
    for (int x = 0; x < (int)chunk.width; x++) {
        for (int z = 0; z < (int)chunk.depth; z++) {
            int worldX = chunk.chunkX * chunk.width + x;
            int worldZ = chunk.chunkZ * chunk.depth + z;

            const float baseHeight = 48.0f;

            const float macroScale = 0.0012f;
            const float macroAmp   = 20.0f;
            float macroN = perlin(worldX * macroScale, worldZ * macroScale);      // [-1,1]
            float macroOffset = macroN * macroAmp;                                 // [-A, A]

            const float regionScale = 0.0035f;
            const float regionAmp   = 6.0f;
            float regionN = perlin(worldX * regionScale + 37.0f, worldZ * regionScale - 91.0f); // [-1,1]
            float regionOffset = regionN * regionAmp;

            const float maskScale = 0.010f;
            float maskRaw = perlin(worldX * maskScale + 200.0f, worldZ * maskScale + 200.0f); // [-1,1]
            float mask01 = (maskRaw + 1.0f) * 0.5f;                                           // [0,1]

            const float maskThreshold = 0.62f;
            const float maskFeather   = 0.08f;
            float hillMask = smoothstepf(maskThreshold, maskThreshold + maskFeather, mask01); // [0,1]

            const float detailScale = 0.05f;
            const float detailAmp   = 2.0f;
            float detailN = perlin(worldX * detailScale - 120.0f, worldZ * detailScale + 53.0f); // [-1,1]
            float detailOffset = detailN * detailAmp;

            const float hillScale = 0.07f;
            const float hillAmp   = 14.0f;
            float hillN = perlin(worldX * hillScale + 777.0f, worldZ * hillScale - 333.0f); // [-1,1]
            float hillOnlyUp = ((hillN + 1.0f) * 0.5f) * hillAmp;                           // [0, hillAmp]
            float hillOffset = hillOnlyUp * hillMask;                                       // sparse strong hills

            int terrainHeight = int(baseHeight + macroOffset + regionOffset + detailOffset + hillOffset);

            if (terrainHeight >= (int)chunk.height) terrainHeight = chunk.height - 1;

            // dirt depth
            float localVariationMag = std::fabs(detailOffset) + hillMask * 0.5f * hillAmp;
            int minDirt = 2;
            int maxDirt = 5;
            int dirtDepth = minDirt + int(clampf(localVariationMag / (hillAmp + detailAmp), 0.0f, 1.0f) * (maxDirt - minDirt));


            int stoneThreshold = int(baseHeight + macroOffset + regionAmp * 0.8f);
            if (terrainHeight > stoneThreshold) {
                dirtDepth = std::max(dirtDepth - (terrainHeight - stoneThreshold) / 2, 1);
            }

            if (dirtDepth > terrainHeight) dirtDepth = terrainHeight;

            for (int y = 0; y < (int)chunk.height; y++) {
                if (y > terrainHeight)
                    chunk.setBlock(x, y, z, AIR);
                else if (y == terrainHeight)
                    chunk.setBlock(x, y, z, GRASS);
                else if (y >= terrainHeight - dirtDepth)
                    chunk.setBlock(x, y, z, DIRT);
                else
                    chunk.setBlock(x, y, z, STONE);
            }
        }
    }
}

void generateTrees(Chunk& chunk, ChunkManager* manager) {
    std::set<std::pair<int,int>> modifiedChunks;

    for (int x = 0; x < (int)chunk.width; x++) {
        for (int z = 0; z < (int)chunk.depth; z++) {
            int worldX = chunk.chunkX * chunk.width + x;
            int worldZ = chunk.chunkZ * chunk.depth + z;

            BiomeType biome = getBiome(worldX, worldZ);

            float chance = (biome == FOREST) ? 0.08f : 0.005f;
            if ((rand() % 1000) / 1000.0f > chance) continue;

            int y;
            for (y = chunk.height - 1; y >= 0; y--) {
                if (chunk.getBlock(x, y, z).type != AIR) break;
            }

            if (y <= 0 || chunk.getBlock(x, y, z).type != GRASS) continue;

            int trunkHeight = 4 + rand() % 3;
            int leafStart = y + trunkHeight - 2;

            int actualTrunkHeight = std::max(1, trunkHeight - 1);
            for (int ty = 1; ty <= actualTrunkHeight; ty++) {
                if (y + ty >= (int)chunk.height) break;
                setBlockWorld(manager, worldX, y + ty, worldZ, WOOD, LogAxis::Y, &modifiedChunks);
            }

            for (int lx = -2; lx <= 2; lx++) {
                for (int lz = -2; lz <= 2; lz++) {
                    for (int ly = 0; ly <= 1; ly++) {
                        int bx = worldX + lx;
                        int bz = worldZ + lz;
                        int by = leafStart + ly;
                        if (by < 0 || by >= (int)chunk.height) continue;

                        int cx = getChunkCoord((float)bx);
                        int cz = getChunkCoord((float)bz);
                        ManagedChunk* target = manager->getChunk(cx, cz);
                        if (!target) continue;

                        int localX = bx - cx * (int)target->chunk.width;
                        int localZ = bz - cz * (int)target->chunk.depth;
                        BlockType current = target->chunk.getBlock(localX, by, localZ).type;
                        if (current == AIR) {
                            setBlockWorld(manager, bx, by, bz, LEAVES, LogAxis::Y, &modifiedChunks);
                        }
                    }
                }
            }

            int baseTopperY = y + actualTrunkHeight +1;
            for (int dy = 0; dy <= 1; ++dy) {
                int by = baseTopperY + dy;
                if (by < 0 || by >= (int)chunk.height) continue;

                {
                    int bx = worldX;
                    int bz = worldZ;
                    int cx = getChunkCoord((float)bx);
                    int cz = getChunkCoord((float)bz);
                    ManagedChunk* target = manager->getChunk(cx, cz);
                    if (target) {
                        int localX = bx - cx * (int)target->chunk.width;
                        int localZ = bz - cz * (int)target->chunk.depth;
                        BlockType current = target->chunk.getBlock(localX, by, localZ).type;
                        if (current == AIR) {
                            setBlockWorld(manager, bx, by, bz, LEAVES, LogAxis::Y, &modifiedChunks);
                        }
                    }
                }

                const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                for (int i = 0; i < 4; ++i) {
                    int bx = worldX + dirs[i][0];
                    int bz = worldZ + dirs[i][1];
                    int cx = getChunkCoord((float)bx);
                    int cz = getChunkCoord((float)bz);
                    ManagedChunk* target = manager->getChunk(cx, cz);
                    if (!target) continue;
                    int localX = bx - cx * (int)target->chunk.width;
                    int localZ = bz - cz * (int)target->chunk.depth;
                    BlockType current = target->chunk.getBlock(localX, by, localZ).type;
                    if (current == AIR) {
                        setBlockWorld(manager, bx, by, bz, LEAVES, LogAxis::Y, &modifiedChunks);
                    }
                }
            }
        }
    }

    for (auto &p : modifiedChunks) {
        ManagedChunk* m = manager->getChunk(p.first, p.second);
        if (m) {
            m->meshDirty = true;
            m->meshUploaded = false;
        }
    }
}

void updateChunks(ChunkManager& manager, glm::vec3 pos, int radius, unsigned int shader) {
    while (true) {
        CompletedTerrain t;
        if (!g_completedTerrain.try_pop(t)) break;
        ManagedChunk* mc = manager.getChunk(t.cx, t.cz);
        if (mc) {
            mc->inTerrainQueue = false;
            mc->terrainGenerated = true;
            mc->meshDirty = true;
        }
    }

    int camChunkX = getChunkCoord(pos.x);
    int camChunkZ = getChunkCoord(pos.z);

    int pad = 1;
    int fullRadius = radius + pad;

    std::set<std::pair<int,int>> shouldExist;
    for (int dx = -fullRadius; dx <= fullRadius; dx++) {
        for (int dz = -fullRadius; dz <= fullRadius; dz++) {
            int cx = camChunkX + dx;
            int cz = camChunkZ + dz;
            shouldExist.insert({cx, cz});
        }
    }

    std::vector<std::pair<int,int>> toRemove;
    for (auto& pair : manager.chunks) {
        if (shouldExist.find(pair.first) == shouldExist.end()) {
            if (!pair.second->inTerrainQueue && !pair.second->inMeshQueue) {
                toRemove.push_back(pair.first);
            }
        }
    }
    for (auto& key : toRemove) {
        manager.removeChunk(key.first, key.second);
    }

    std::vector<std::pair<int,int>> newlyCreated;
    for (auto& p : shouldExist) {
        int cx = p.first;
        int cz = p.second;
        if (!manager.getChunk(cx, cz)) {
            ManagedChunk* mc = new ManagedChunk(cx, cz);
            manager.addChunk(cx, cz, mc);
            newlyCreated.emplace_back(cx, cz);
        }
    }

    // TERRAIN PASS
    for (auto& pair : manager.chunks) {
        ManagedChunk* mc = pair.second;
        if (!mc->terrainGenerated && !mc->inTerrainQueue) {
            mc->inTerrainQueue = true;
            int cx = mc->chunk.chunkX;
            int cz = mc->chunk.chunkZ;

            getThreadPool().enqueue([mc, cx, cz]() {
                generateTerrainForChunk(mc->chunk);
                g_completedTerrain.push({cx, cz});
            });
        }
    }

    // STRUCTURE PASS
    std::set<std::pair<int,int>> modifiedByStructures;
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dz = -radius; dz <= radius; dz++) {
            if (dx*dx + dz*dz > radius*radius) continue;
            int cx = camChunkX + dx;
            int cz = camChunkZ + dz;

            ManagedChunk* mc = manager.getChunk(cx, cz);
            if (!mc) continue;

            if (mc->terrainGenerated && !mc->structuresGenerated) {
                bool neighborsReady = true;
                for (int nx = -1; nx <= 1; ++nx) {
                    for (int nz = -1; nz <= 1; ++nz) {
                        if (nx == 0 && nz == 0) continue;
                        ManagedChunk* n = manager.getChunk(cx + nx, cz + nz);
                        if (!n || !n->terrainGenerated || n->inTerrainQueue) {
                            neighborsReady = false;
                            break;
                        }
                    }
                    if (!neighborsReady) break;
                }

                if (neighborsReady) {
                    generateTrees(mc->chunk, &manager);
                    mc->structuresGenerated = true;
                    mc->meshDirty = true;
                }
            }
        }
    }

    // MESH PASS
    for (auto& p : shouldExist) {
        auto mc = manager.getChunk(p.first, p.second);
        if (!mc) continue;

        if (!mc->terrainGenerated) continue;
        if (!mc->structuresGenerated) continue;

        if ((!mc->meshUploaded || mc->meshDirty) && !mc->inMeshQueue) {
            mc->inMeshQueue = true;
            int cx = p.first;
            int cz = p.second;
            getThreadPool().enqueue([cx, cz, &manager]() {
                auto m = manager.getChunk(cx, cz);
                if (!m) return;
                auto verts = ChunkMesh::buildVertices(m->chunk, &manager);
                g_completedMeshes.push({cx, cz, std::move(verts)});
            });
        }
    }
}