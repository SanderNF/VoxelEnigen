#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;

    float yaw;
    float pitch;
    float lastX, lastY;
    bool firstMouse;
    float movementSpeed;

    Camera(glm::vec3 startPos = glm::vec3(0.0f, 75.0f, 0.0f));

    void processMouseMovement(float xoffset, float yoffset, float sensitivity = 0.1f);
    void processKeyboard(GLFWwindow* window, float deltaTime);

    glm::mat4 getViewMatrix() const;

private:
    void updateCameraVectors();
};