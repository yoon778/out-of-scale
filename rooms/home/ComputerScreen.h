#pragma once

#include "../shared/BoxRenderer.h"

class ComputerScreen {
public:
    ComputerScreen() = default;
    ComputerScreen(const ComputerScreen&) = delete;
    ComputerScreen& operator=(const ComputerScreen&) = delete;
    ~ComputerScreen();

    bool initialize();
    void input(float horizontal, float vertical, float turn, float elapsed);
    void scroll(double amount);
    void setCubeVisible(bool visible);
    void setCube(float edge, float yaw);
    float edge() const;
    float yaw() const;
    void draw(const glm::mat4& viewProjection) const;
    void drawCube(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawGuide(bool active, bool holding) const;

    static constexpr glm::vec3 center{5.505f, 1.85f, 3.0f};

private:
    void clampPosition();
    void renderTexture() const;
    void drawTexture(GLuint texture, const glm::mat4& transform, bool flip) const;

    BoxRenderer renderer_;
    GLuint framebuffer_ = 0;
    GLuint color_ = 0;
    GLuint depth_ = 0;
    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    GLuint guide_ = 0;
    GLuint holdingGuide_ = 0;
    GLint transform_ = -1;
    GLint flip_ = -1;
    glm::vec2 position_{0.0f};
    float edge_ = 0.60f;
    float yaw_ = 0.35f;
    bool visible_ = true;
    mutable bool dirty_ = true;
};
