#include "player.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "imgui/imgui.h"

Player::Player(glm::vec3 startPos)
    : position(startPos),
      velocity(0.0f),
      front(glm::vec3(0.0f, 0.0f, -1.0f)),
      up(glm::vec3(0.0f, 1.0f, 0.0f)),
      yaw(-90.0f),
      pitch(0.0f),
      lastX(400.0f),
      lastY(300.0f),
      firstMouse(true),
      mode(MovementMode::FLY),
      onGround(false),
      verticalVelocity(0.0f)
{
    updateVectors();
}

void Player::processMouseMovement(float xoffset, float yoffset, float sensitivity) {
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    updateVectors();
}

void Player::processKeyboard(GLFWwindow* window, float deltaTime, ChunkManager* world) {
    float speed = 0.0f;

    if (mode == MovementMode::FLY) {
        speed = flySpeed * deltaTime;

        glm::vec3 movement(0.0f);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            movement += front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            movement -= front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            movement -= right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            movement += right;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            movement += up;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            movement -= up;

        if (glm::length(movement) > 0.0f) {
            movement = glm::normalize(movement) * speed;
            position += movement;
        }
    } else {
        bool sprinting = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        speed = (sprinting ? sprintSpeed : walkSpeed);

        glm::vec3 forward;
        forward.x = cos(glm::radians(yaw));
        forward.y = 0.0f;
        forward.z = sin(glm::radians(yaw));
        forward = glm::normalize(forward);

        glm::vec3 rightDir = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

        glm::vec3 movement(0.0f);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            movement += forward;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            movement -= forward;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            movement -= rightDir;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            movement += rightDir;

        if (glm::length(movement) > 0.0f) {
            movement = glm::normalize(movement) * speed;
            velocity.x = movement.x;
            velocity.z = movement.z;
        } else {
            velocity.x = 0.0f;
            velocity.z = 0.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && onGround) {
            verticalVelocity = jumpVelocity;
            onGround = false;
        }
    }

}

void Player::update(float deltaTime, ChunkManager* world) {
    if (mode == MovementMode::NORMAL) {
        verticalVelocity += gravity * deltaTime;

        glm::vec3 desiredPos = position;
        desiredPos.x += velocity.x * deltaTime;
        desiredPos.z += velocity.z * deltaTime;
        desiredPos.y += verticalVelocity * deltaTime;

        glm::vec3 newPos = resolveCollision(desiredPos, world);

        if (newPos.y == position.y && verticalVelocity < 0.0f) {
            onGround = true;
            verticalVelocity = 0.0f;
        } else if (newPos.y == position.y && verticalVelocity > 0.0f) {
            verticalVelocity = 0.0f;
        }

        position = newPos;
    }
}

// Interaction
const std::vector<BlockType>& Player::blockList() {
    static const std::vector<BlockType> list = { AIR, GRASS, DIRT, STONE, WOOD, LEAVES };
    return list;
}

const char* Player::blockName(BlockType t) {
    switch (t) {
        case AIR: return "Air";
        case GRASS: return "Grass";
        case DIRT: return "Dirt";
        case STONE: return "Stone";
        case WOOD: return "Wood";
        case LEAVES: return "Leaves";
        default: return "Unknown";
    }
}

bool Player::raycastBlock(float maxDist, glm::ivec3& outBlock, glm::ivec3& outNormal) const {
    if (!worldRef) return false;
    glm::vec3 rayOrigin = getCameraPosition() + raycastOriginOffset;
    glm::vec3 rayDir = glm::normalize(front);

    glm::ivec3 mapPos = glm::ivec3(glm::floor(rayOrigin));
    glm::vec3 deltaDist = glm::abs(glm::vec3(
        1.0f / (rayDir.x == 0.0f ? 1e-6f : rayDir.x),
        1.0f / (rayDir.y == 0.0f ? 1e-6f : rayDir.y),
        1.0f / (rayDir.z == 0.0f ? 1e-6f : rayDir.z)));

    glm::ivec3 step(
        rayDir.x < 0 ? -1 : 1,
        rayDir.y < 0 ? -1 : 1,
        rayDir.z < 0 ? -1 : 1
    );

    glm::vec3 sideDist;
    sideDist.x = ((rayDir.x < 0 ? (rayOrigin.x - (float)mapPos.x) : ((float)mapPos.x + 1.0f - rayOrigin.x))) * deltaDist.x;
    sideDist.y = ((rayDir.y < 0 ? (rayOrigin.y - (float)mapPos.y) : ((float)mapPos.y + 1.0f - rayOrigin.y))) * deltaDist.y;
    sideDist.z = ((rayDir.z < 0 ? (rayOrigin.z - (float)mapPos.z) : ((float)mapPos.z + 1.0f - rayOrigin.z))) * deltaDist.z;

    glm::ivec3 hitNormal(0);
    float dist = 0.0f;
    const float maxDistEps = maxDist + 1.0f;

    while (dist <= maxDistEps) {
        int cx = getChunkCoord((float)mapPos.x);
        int cz = getChunkCoord((float)mapPos.z);
        ManagedChunk* mc = worldRef->getChunk(cx, cz);
        if (mc) {
            int localX = mapPos.x - cx * (int)mc->chunk.width;
            int localZ = mapPos.z - cz * (int)mc->chunk.depth;
            if (localX >= 0 && localX < (int)mc->chunk.width &&
                localZ >= 0 && localZ < (int)mc->chunk.depth &&
                mapPos.y >= 0 && mapPos.y < (int)mc->chunk.height) {
                BlockType bt = mc->chunk.getBlock(localX, mapPos.y, localZ).type;
                if (bt != AIR) {
                    outBlock = mapPos;
                    outNormal = hitNormal;
                    return true;
                }
            }
        }

        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) {
                mapPos.x += step.x;
                dist = sideDist.x;
                sideDist.x += deltaDist.x;
                hitNormal = glm::ivec3(-step.x, 0, 0);
            } else {
                mapPos.z += step.z;
                dist = sideDist.z;
                sideDist.z += deltaDist.z;
                hitNormal = glm::ivec3(0, 0, -step.z);
            }
        } else {
            if (sideDist.y < sideDist.z) {
                mapPos.y += step.y;
                dist = sideDist.y;
                sideDist.y += deltaDist.y;
                hitNormal = glm::ivec3(0, -step.y, 0);
            } else {
                mapPos.z += step.z;
                dist = sideDist.z;
                sideDist.z += deltaDist.z;
                hitNormal = glm::ivec3(0, 0, -step.z);
            }
        }
    }
    return false;
}

void Player::rebuildChunkMesh(int worldX, int worldZ) {
    if (!worldRef) return;
    int cx = getChunkCoord((float)worldX);
    int cz = getChunkCoord((float)worldZ);
    ManagedChunk* mc = worldRef->getChunk(cx, cz);
    if (mc) {
        mc->mesh.generateMesh(mc->chunk, worldRef);
        mc->meshDirty = false;
        mc->meshUploaded = true;
    }
}

void Player::rebuildNeighborsIfEdge(int worldX, int y, int worldZ) {
    if (!worldRef) return;
    int cx = getChunkCoord((float)worldX);
    int cz = getChunkCoord((float)worldZ);
    ManagedChunk* mc = worldRef->getChunk(cx, cz);
    if (!mc) return;
    int localX = worldX - cx * (int)mc->chunk.width;
    int localZ = worldZ - cz * (int)mc->chunk.depth;
    rebuildChunkMesh(worldX, worldZ);
    if (localX == 0) rebuildChunkMesh(worldX - 1, worldZ);
    if (localX == (int)mc->chunk.width - 1) rebuildChunkMesh(worldX + 1, worldZ);
    if (localZ == 0) rebuildChunkMesh(worldX, worldZ - 1);
    if (localZ == (int)mc->chunk.depth - 1) rebuildChunkMesh(worldX, worldZ + 1);
}

void Player::handleMouseButton(int button, int action, int mods) {
    if (action != GLFW_PRESS || !worldRef) return;
    glm::ivec3 hitBlock, hitNormal;
    if (raycastBlock(6.0f, hitBlock, hitNormal)) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            setBlockWorld(worldRef, hitBlock.x, hitBlock.y, hitBlock.z, AIR);
            rebuildNeighborsIfEdge(hitBlock.x, hitBlock.y, hitBlock.z);
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            BlockType sel = blockList()[selectedBlock];
            if (sel != AIR) {
                glm::ivec3 placePos = hitBlock + hitNormal;
                AABB playerBox = getAABB();
                AABB blockBox(glm::vec3(placePos.x + 0.5f, placePos.y, placePos.z + 0.5f), 1.0f, 1.0f, 1.0f);
                if (!playerBox.intersects(blockBox)) {
                    setBlockWorld(worldRef, placePos.x, placePos.y, placePos.z, sel);
                    rebuildNeighborsIfEdge(placePos.x, placePos.y, placePos.z);
                }
            }
        }
    }
}

void Player::handleScroll(double yoffset) {
    const auto& list = blockList();
    if (yoffset > 0) {
        selectedBlock = (selectedBlock + 1) % (int)list.size();
    } else if (yoffset < 0) {
        selectedBlock = (selectedBlock - 1 + (int)list.size()) % (int)list.size();
    }
}

void Player::renderHUD() {
    ImGui::SetNextWindowBgAlpha(0.25f);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::Begin("HUD", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav);
    BlockType sel = blockList()[selectedBlock];
    ImGui::Text("Selected: %s", blockName(sel));
    ImGui::End();
}

AABB Player::getAABB() const {
    return AABB(position, width, height, depth);
}

void Player::toggleMode() {
    if (mode == MovementMode::FLY) {
        mode = MovementMode::NORMAL;
        verticalVelocity = 0.0f;
    } else {
        mode = MovementMode::FLY;
        velocity = glm::vec3(0.0f);
        verticalVelocity = 0.0f;
        onGround = false;
    }
}

void Player::updateVectors() {
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(direction);
    right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, front));
}

glm::mat4 Player::getViewMatrix() const {
    glm::vec3 cameraPos = getCameraPosition();
    return glm::lookAt(cameraPos, cameraPos + front, up);
}

glm::vec3 Player::getCameraPosition() const {
    return position + glm::vec3(-0.5f, eyeHeight - 0.5f, -0.5f);
}

glm::vec3 Player::resolveCollision(glm::vec3 desiredPos, ChunkManager* world) {
    const float epsilon = 0.001f;
    glm::vec3 result = desiredPos;
    
    glm::vec3 testPos = position;
    testPos.x = desiredPos.x;
    if (checkCollision(testPos, world)) {
        result.x = position.x;
    }
    
    testPos = position;
    testPos.z = desiredPos.z;
    if (checkCollision(testPos, world)) {
        result.z = position.z;
    }
    
    testPos = position;
    testPos.y = desiredPos.y;
    if (checkCollision(testPos, world)) {
        result.y = position.y;
    }
    
    return result;
}

bool Player::checkCollision(glm::vec3 newPos, ChunkManager* world) {
    AABB playerBox(newPos, width, height, depth);
    
    int minX = (int)floor(playerBox.min.x);
    int maxX = (int)ceil(playerBox.max.x);
    int minY = (int)floor(playerBox.min.y);
    int maxY = (int)ceil(playerBox.max.y);
    int minZ = (int)floor(playerBox.min.z);
    int maxZ = (int)ceil(playerBox.max.z);
    
    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                int cx = getChunkCoord((float)x);
                int cz = getChunkCoord((float)z);
                
                ManagedChunk* chunk = world->getChunk(cx, cz);
                if (!chunk) continue;
                
                int localX = x - cx * chunk->chunk.width;
                int localZ = z - cz * chunk->chunk.depth;
                
                if (localX < 0 || localX >= (int)chunk->chunk.width ||
                    localZ < 0 || localZ >= (int)chunk->chunk.depth ||
                    y < 0 || y >= (int)chunk->chunk.height) {
                    continue;
                }
                
                BlockType blockType = chunk->chunk.getBlock(localX, y, localZ).type;
                
                if (isBlockSolid(blockType)) {
                    AABB blockBox(glm::vec3(x + 0.5f, y, z + 0.5f), 1.0f, 1.0f, 1.0f);
                    if (playerBox.intersects(blockBox)) {
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

bool Player::isBlockSolid(BlockType type) {
    return type != AIR;
}