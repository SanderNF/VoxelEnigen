#include "chunk.h"
#include "world.h"
#include "texture_atlas.h"

float cubeFaces[6][30] = {
    // ---------- FRONT -Z ----------
    {
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,

         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
    },
    // ---------- BACK +Z ----------
    {
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,

         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f
    },
    // ---------- LEFT -X ----------
    {
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f
    },
    // ---------- RIGHT +X ----------
    {
        0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,

        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  0.0f, 0.0f
    },
    // ---------- BOTTOM -Y ----------
    {
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f,

         0.5f, -0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f
     },
     // ---------- TOP +Y ----------
     {
         -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,
          0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
          0.5f,  0.5f, -0.5f,  1.0f, 0.0f,

          0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
         -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,
         -0.5f,  0.5f,  0.5f,  0.0f, 1.0f
      }
};

Chunk::Chunk(int cx, int cz, unsigned int w, unsigned int d, unsigned int h)
    : chunkX(cx), chunkZ(cz), width(w), depth(d), height(h) {
    blocks.resize(width * height * depth);
}

Block& Chunk::getBlock(int x, int y, int z) {
    return blocks[x + width * (y + height * z)];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    blocks[x + width * (y + height * z)].type = type;
}

void ChunkMesh::appendFaceWithAtlas(float face[30], int x, int y, int z, int chunkX, int chunkZ,
                                   int chunkWidth, int chunkDepth, Block& block, int faceIndex) {
    float worldX = chunkX * chunkWidth + x;
    float worldZ = chunkZ * chunkDepth + z;

    AtlasTexture tex = g_textureAtlas.getTexture(block.type, faceIndex);
    const AtlasConfig& config = g_textureAtlas.getConfig();

    float uScale = config.getUScale();
    float vScale = config.getVScale();
    float uOffset = g_textureAtlas.getUOffset(tex);
    float vOffset = g_textureAtlas.getVOffset(tex);

    for (int i = 0; i < 6; ++i) {
        float u = face[i*5 + 3];
        float v = face[i*5 + 4];

        // Handle log rotation for wood blocks
        if (block.type == WOOD) {
            if (faceIndex < 4) { // Side faces
                if (block.axis == LogAxis::X) std::swap(u, v);
                else if (block.axis == LogAxis::Z) u = 1.0f - u;
            }
        }

        vertices.push_back(face[i*5 + 0] + worldX);
        vertices.push_back(face[i*5 + 1] + y);
        vertices.push_back(face[i*5 + 2] + worldZ);
        vertices.push_back(u * uScale + uOffset);
        vertices.push_back(v * vScale + vOffset);
    }
}

void ChunkMesh::uploadToGPU(const std::vector<float>& newVertices) {
    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);

    vertices = newVertices;

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void ChunkMesh::draw() {
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 5);
}

std::vector<float> ChunkMesh::buildVertices(Chunk& chunk, ChunkManager* manager) {
    ChunkMesh tmp;
    tmp.vertices.clear();

    ManagedChunk* neighborLeft  = manager->getChunk(chunk.chunkX - 1, chunk.chunkZ);
    ManagedChunk* neighborRight = manager->getChunk(chunk.chunkX + 1, chunk.chunkZ);
    ManagedChunk* neighborFront = manager->getChunk(chunk.chunkX, chunk.chunkZ - 1);
    ManagedChunk* neighborBack  = manager->getChunk(chunk.chunkX, chunk.chunkZ + 1);

    auto isAir = [&](int bx, int by, int bz) -> bool {
        if (by < 0 || by >= (int)chunk.height) return true;

        if (bx < 0)  return !neighborLeft  || neighborLeft->chunk.getBlock(chunk.width-1, by, bz).type == AIR;
        if (bx >= (int)chunk.width)  return !neighborRight || neighborRight->chunk.getBlock(0, by, bz).type == AIR;
        if (bz < 0)  return !neighborFront || neighborFront->chunk.getBlock(bx, by, chunk.depth-1).type == AIR;
        if (bz >= (int)chunk.depth)  return !neighborBack  || neighborBack->chunk.getBlock(bx, by, 0).type == AIR;

        return chunk.getBlock(bx, by, bz).type == AIR;
    };

    for (int x = 0; x < (int)chunk.width; x++) {
        for (int y = 0; y < (int)chunk.height; y++) {
            for (int z = 0; z < (int)chunk.depth; z++) {
                Block& block = chunk.getBlock(x, y, z);
                if (block.type == AIR) continue;

                if (isAir(x, y, z - 1)) tmp.appendFaceWithAtlas(cubeFaces[0], x, y, z, chunk.chunkX, chunk.chunkZ, chunk.width, chunk.depth, block, 0);
                if (isAir(x, y, z + 1)) tmp.appendFaceWithAtlas(cubeFaces[1], x, y, z, chunk.chunkX, chunk.chunkZ, chunk.width, chunk.depth, block, 1);
                if (isAir(x - 1, y, z)) tmp.appendFaceWithAtlas(cubeFaces[2], x, y, z, chunk.chunkX, chunk.chunkZ, chunk.width, chunk.depth, block, 2);
                if (isAir(x + 1, y, z)) tmp.appendFaceWithAtlas(cubeFaces[3], x, y, z, chunk.chunkX, chunk.chunkZ, chunk.width, chunk.depth, block, 3);
                if (isAir(x, y - 1, z)) tmp.appendFaceWithAtlas(cubeFaces[4], x, y, z, chunk.chunkX, chunk.chunkZ, chunk.width, chunk.depth, block, 4);
                if (isAir(x, y + 1, z)) tmp.appendFaceWithAtlas(cubeFaces[5], x, y, z, chunk.chunkX, chunk.chunkZ, chunk.width, chunk.depth, block, 5);
            }
        }
    }

    return std::move(tmp.vertices);
}

void ChunkMesh::generateMesh(Chunk& chunk, ChunkManager* manager) {
    auto built = ChunkMesh::buildVertices(chunk, manager);
    uploadToGPU(built);
}

ManagedChunk::ManagedChunk(int cx, int cz) : chunk(cx, cz, 16, 16, 128) {}