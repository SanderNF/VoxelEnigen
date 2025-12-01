#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include "world.h"

enum class MovementMode {
    FLY,
    NORMAL
};

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    AABB(glm::vec3 center, float width, float height, float depth) {
        min = center - glm::vec3(width/2.0f, 0.0f, depth/2.0f);
        max = center + glm::vec3(width/2.0f, height, depth/2.0f);
    }

    bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }
};

class Player {
public:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    
    float yaw;
    float pitch;
    float lastX, lastY;
    bool firstMouse;
    
    float width = 0.8f;
    float height = 1.85f;
    float depth = 0.8f;
    float eyeHeight = 1.62f;
    
    MovementMode mode;
    bool onGround;
    float verticalVelocity;
    
    // Movement
    float walkSpeed = 4.3f;
    float sprintSpeed = 5.6f;
    float flySpeed = 10.0f;
    float jumpVelocity = 8.0f;
    float gravity = -20.0f;
    
    Player(glm::vec3 startPos = glm::vec3(0.0f, 75.0f, 0.0f));
    
    void processMouseMovement(float xoffset, float yoffset, float sensitivity = 0.1f);
    void processKeyboard(GLFWwindow* window, float deltaTime, ChunkManager* world);
    void update(float deltaTime, ChunkManager* world);
    
    glm::mat4 getViewMatrix() const;
    glm::vec3 getCameraPosition() const;
    AABB getAABB() const;
    
    void toggleMode();

    void setActiveWorld(ChunkManager* world) { worldRef = world; }
    void handleMouseButton(int button, int action, int mods);
    void handleScroll(double yoffset);
    void renderHUD();
    void setRaycastOriginOffset(const glm::vec3& offset) { raycastOriginOffset = offset; }

private:
    void updateVectors();
    bool checkCollision(glm::vec3 newPos, ChunkManager* world);
    glm::vec3 resolveCollision(glm::vec3 desiredPos, ChunkManager* world);
    bool isBlockSolid(BlockType type);

    bool raycastBlock(float maxDist, glm::ivec3& outBlock, glm::ivec3& outNormal) const;
    void rebuildChunkMesh(int worldX, int worldZ);
    void rebuildNeighborsIfEdge(int worldX, int y, int worldZ);

    int selectedBlock = 1;
    ChunkManager* worldRef = nullptr;
    static const char* blockName(BlockType t);
    static const std::vector<BlockType>& blockList();

    glm::vec3 raycastOriginOffset = glm::vec3(0.0f);
};