#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <limits>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "player.h"
#include "renderer.h"
#include "world.h"
#include "chunk.h"

Player* g_player = nullptr;

int g_windowWidth = 400;
int g_windowHeight = 200;
float g_aspectRatio = 16.0f / 9.0f;

bool g_showPauseMenu = false;
bool g_showSettings = false;

bool g_vsyncEnabled = false;
int g_fpsLimit = 0;
int g_selectedResolution = 0;

bool g_debugHitbox = false;


struct Resolution {
    int width;
    int height;
    std::string label;
};
std::vector<Resolution> g_resolutions;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;

    g_windowWidth = width;
    g_windowHeight = height;
    g_aspectRatio = (float)width / (float)height;

    const float minAspect = 0.1f;
    const float maxAspect = 10.0f;
    g_aspectRatio = glm::clamp(g_aspectRatio, minAspect, maxAspect);

    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!g_player || g_showPauseMenu) return;

    if (g_player->firstMouse) {
        g_player->lastX = xpos;
        g_player->lastY = ypos;
        g_player->firstMouse = false;
    }

    float xoffset = xpos - g_player->lastX;
    float yoffset = g_player->lastY - ypos;

    g_player->lastX = xpos;
    g_player->lastY = ypos;

    g_player->processMouseMovement(xoffset, yoffset);
}
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (!g_player || g_showPauseMenu) return;
    g_player->handleMouseButton(button, action, mods);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (g_showPauseMenu) return;
    if (!g_player) return;
    g_player->handleScroll(yoffset);
}

void togglePauseMenu(GLFWwindow* window) {
    g_showPauseMenu = !g_showPauseMenu;

    if (g_showPauseMenu) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        g_player->firstMouse = true;
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        g_showSettings = false;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            togglePauseMenu(window);
        }

        if (key == GLFW_KEY_F && g_player) {
            g_player->toggleMode();
            std::cout << "Movement mode: " << (g_player->mode == MovementMode::FLY ? "FLY" : "NORMAL") << std::endl;
        }

        if (key == GLFW_KEY_F11) {
            static bool isFullscreen = false;
            static int windowedX, windowedY, windowedWidth, windowedHeight;

            if (!isFullscreen) {
                glfwGetWindowPos(window, &windowedX, &windowedY);
                glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

                GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);

                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                isFullscreen = true;
            } else {
                glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
                isFullscreen = false;
            }
        }
    }
}

void initializeResolutions(GLFWmonitor* monitor) {
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int nativeWidth = mode->width;
    int nativeHeight = mode->height;

    std::vector<std::pair<int, int>> commonRes = {
        {1280, 720},
        {1920, 1080},
        {2560, 1440},
        {3840, 2160},
        {1366, 768},
        {1600, 900},
        {1680, 1050},
        {2560, 1080}
    };

    g_resolutions.push_back({nativeWidth, nativeHeight,
        std::to_string(nativeWidth) + "x" + std::to_string(nativeHeight) + " (Native - Recommended)"});

    for (auto& res : commonRes) {
        if (res.first <= nativeWidth && res.second <= nativeHeight) {
            if (res.first != nativeWidth || res.second != nativeHeight) {
                g_resolutions.push_back({res.first, res.second,
                    std::to_string(res.first) + "x" + std::to_string(res.second)});
            }
        }
    }

    for (size_t i = 0; i < g_resolutions.size(); i++) {
        if (g_resolutions[i].width == g_windowWidth && g_resolutions[i].height == g_windowHeight) {
            g_selectedResolution = i;
            break;
        }
    }
}

void applyResolution(GLFWwindow* window, int index) {
    if (index < 0 || index >= g_resolutions.size()) return;

    Resolution& res = g_resolutions[index];
    glfwSetWindowSize(window, res.width, res.height);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowPos(window, (mode->width - res.width) / 2, (mode->height - res.height) / 2);
}

void renderPauseMenu(GLFWwindow* window) {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);

    ImGui::Begin("Pause Menu", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::SetWindowFontScale(1.5f);

    if (ImGui::Button("Resume", ImVec2(-1, 50))) {
        togglePauseMenu(window);
    }

    ImGui::Spacing();

    if (ImGui::Button("Settings", ImVec2(-1, 50))) {
        g_showSettings = true;
    }

    ImGui::Spacing();

    if (ImGui::Button("Quit", ImVec2(-1, 50))) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    ImGui::End();
}

void renderSettingsMenu(GLFWwindow* window) {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Always);

    ImGui::Begin("Settings", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Graphics Settings");
    ImGui::Separator();
    ImGui::Spacing();

    bool vsyncChanged = ImGui::Checkbox("VSync", &g_vsyncEnabled);
    if (vsyncChanged) {
        glfwSwapInterval(g_vsyncEnabled ? 1 : 0);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Synchronize framerate with monitor refresh rate");
    }

    ImGui::Spacing();

    ImGui::Text("FPS Limit:");
    ImGui::SameLine();
    const char* fpsItems[] = { "Unlimited", "30", "60", "120", "144", "240" };
    static int fpsIndex = 0;
    if (ImGui::Combo("##FPS", &fpsIndex, fpsItems, IM_ARRAYSIZE(fpsItems))) {
        int fpsValues[] = {0, 30, 60, 120, 144, 240};
        g_fpsLimit = fpsValues[fpsIndex];
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Resolution:");
    if (ImGui::BeginCombo("##Resolution", g_resolutions[g_selectedResolution].label.c_str())) {
        for (int i = 0; i < g_resolutions.size(); i++) {
            bool isSelected = (g_selectedResolution == i);
            if (ImGui::Selectable(g_resolutions[i].label.c_str(), isSelected)) {
                g_selectedResolution = i;
                applyResolution(window, i);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Player Settings");
    ImGui::Separator();
    ImGui::Spacing();

    if (g_player) {
        ImGui::Text("Movement Mode:");
        ImGui::SameLine();
        ImGui::Text("%s", g_player->mode == MovementMode::FLY ? "FLY" : "NORMAL");
        ImGui::Text("Press F to toggle");

        ImGui::Spacing();

        ImGui::Text("Fly Speed:");
        ImGui::SliderFloat("##FlySpeed", &g_player->flySpeed, 5.0f, 50.0f, "%.1f");

        ImGui::Text("Walk Speed:");
        ImGui::SliderFloat("##WalkSpeed", &g_player->walkSpeed, 2.0f, 10.0f, "%.1f");

        ImGui::Text("Sprint Speed:");
        ImGui::SliderFloat("##SprintSpeed", &g_player->sprintSpeed, 4.0f, 15.0f, "%.1f");
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Back", ImVec2(-1, 40))) {
        g_showSettings = false;
    }

    ImGui::End();
}


int main(int argc, char* argv[]) {
    unsigned int seed = 0;
    if (argc > 1) {
        try {
            seed = std::stoul(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid seed provided, using random seed" << std::endl;
            seed = static_cast<unsigned int>(std::time(nullptr));
        }
    } else {
        seed = static_cast<unsigned int>(std::time(nullptr));
    }

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    std::cout << "Using world seed: " << seed << std::endl;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    GLFWwindow* window = glfwCreateWindow(g_windowWidth, g_windowHeight, "Voxel Terrain", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetWindowPos(window, (mode->width - g_windowWidth) / 2, (mode->height - g_windowHeight) / 2);

    glfwMakeContextCurrent(window);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSwapInterval(g_vsyncEnabled ? 1 : 0);

    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glViewport(0, 0, g_windowWidth, g_windowHeight);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    initializeResolutions(monitor);

    Player player(glm::vec3(0.0f, 75.0f, 0.0f));
    g_player = &player;

    Renderer renderer;
    if (!renderer.initialize()) {
        std::cout << "Failed to initialize renderer" << std::endl;
        return -1;
    }

    initPerlin(seed);
    ChunkManager chunkManager;
    player.setActiveWorld(&chunkManager);
    player.setRaycastOriginOffset(glm::vec3(0.5f, 0.5f, 0.5f));

    int renderDistance = 8;
    updateChunks(chunkManager, player.position, renderDistance, renderer.getShaderProgram());

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float lastTime = glfwGetTime();
    int frames = 0;
    float frameTimeAccumulator = 0.0f;

    int lastCamChunkX = getChunkCoord(player.position.x);
    int lastCamChunkZ = getChunkCoord(player.position.z);

    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD - Move" << std::endl;
    std::cout << "  Space - Jump (Normal mode) / Up (Fly mode)" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;

        if (g_fpsLimit > 0) {
            float targetFrameTime = 1.0f / g_fpsLimit;
            frameTimeAccumulator += deltaTime;

            if (frameTimeAccumulator < targetFrameTime) {
                continue;
            }
            frameTimeAccumulator = 0.0f;
        }

        lastFrame = currentFrame;

        frames++;
        float currentTime = glfwGetTime();
        if (currentTime - lastTime >= 1.0f) {
            std::cout << "FPS: " << frames
                      << " | Chunks: " << chunkManager.chunks.size()
                      << " | Mode: " << (player.mode == MovementMode::FLY ? "FLY" : "NORMAL")
                      << " | Pos: (" << (int)player.position.x << ", " << (int)player.position.y << ", " << (int)player.position.z << ")"
                      << std::endl;
            frames = 0;
            lastTime += 1.0f;
        }

        if (!g_showPauseMenu) {
            player.processKeyboard(window, deltaTime, &chunkManager);
            player.update(deltaTime, &chunkManager);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = player.getViewMatrix();
        // Ensure aspect ratio is valid before creating projection matrix
        float safeAspect = g_aspectRatio;
        if (safeAspect <= 0.0f) {
            safeAspect = 16.0f / 9.0f;  // Fallback to default aspect ratio
        }
        glm::mat4 projection = glm::perspective(
            glm::radians(70.0f),
            safeAspect,
            0.1f,
            500.0f
        );

        glUseProgram(renderer.getShaderProgram());
        glUniformMatrix4fv(
            glGetUniformLocation(renderer.getShaderProgram(), "view"),
            1, GL_FALSE, glm::value_ptr(view)
        );
        glUniformMatrix4fv(
            glGetUniformLocation(renderer.getShaderProgram(), "projection"),
            1, GL_FALSE, glm::value_ptr(projection)
        );

        updateChunks(chunkManager, player.position, renderDistance, renderer.getShaderProgram());

        while (true) {
            CompletedMesh m;
            if (!g_completedMeshes.try_pop(m)) break;
            ManagedChunk* mc = chunkManager.getChunk(m.cx, m.cz);
            if (!mc) continue;
            mc->mesh.uploadToGPU(m.vertices);
            mc->meshDirty = false;
            mc->meshUploaded = true;
            mc->inMeshQueue = false;
        }

        glBindTexture(GL_TEXTURE_2D, renderer.getAtlasTexture());
        for (auto& pair : chunkManager.chunks) {
            ManagedChunk* mc = pair.second;

            glm::mat4 model = glm::mat4(1.0f);
            glUniformMatrix4fv(
                glGetUniformLocation(renderer.getShaderProgram(),
                "model"),
1, GL_FALSE, glm::value_ptr(model)
                );
            mc->mesh.draw();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (g_showPauseMenu) {
            if (g_showSettings) {
                renderSettingsMenu(window);
            } else {
                renderPauseMenu(window);
            }
        }

        if (!g_showPauseMenu) {
            player.renderHUD();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    std::cout << "\nPress Enter to close...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    return 0;
}

