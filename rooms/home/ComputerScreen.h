#pragma once

#include "../shared/BoxRenderer.h"
#include "CourseworkLabs.h"

#include <array>

class ComputerScreen {
public:
    enum class Action { None, TakeCompleteKey };
    enum class CarryHint { None, Common, CompleteKey, Socket, Miniature };
    ComputerScreen() = default;
    ComputerScreen(const ComputerScreen&) = delete;
    ComputerScreen& operator=(const ComputerScreen&) = delete;
    ~ComputerScreen();

    bool initialize();
    void keyInput(int key, int action, int mods);
    void pointer(float u, float v); // Whole-window coordinates, top-left origin.
    void mouseButton(int button, int action, int mods);
    void scroll(double amount);
    void update(float elapsed);
    void selectLesson(int zeroBased);
    int selectedLesson() const;
    Action confirm();
    void consumeKey(); // Only after the world successfully receives the key.
    bool keyTaken() const;
    void drawWorkspace() const;
    void draw(const glm::mat4& viewProjection) const;
    void drawCube(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawKeyPiece(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawCompleteKey(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawGuide(bool active, bool holding, bool holdingKey = false) const;
    void drawCarryGuide(CarryHint hint) const;
    void clearInput();
    static constexpr glm::vec3 center{5.505f, 1.85f, 3.0f};

private:
    void renderTexture() const;
    void drawTexture(GLuint texture, const glm::mat4& transform, bool flip) const;
    void drawLabel(GLuint texture) const;

    BoxRenderer renderer_;
    CourseworkLabs labs_;
    GLuint framebuffer_ = 0;
    GLuint color_ = 0;
    GLuint depth_ = 0;
    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    GLuint holdingGuide_ = 0;
    GLuint completeKeyGuide_ = 0;
    GLuint socketGuide_ = 0;
    GLuint miniatureGuide_ = 0;
    std::array<GLuint, 6> workspaceLabels_{};
    std::array<GLuint, 6> helpLabels_{};
    GLuint keyReadyLabel_ = 0;
    GLuint keyTakenLabel_ = 0;
    GLuint keyRejectedLabel_ = 0;
    GLint transform_ = -1;
    GLint flip_ = -1;
    glm::vec2 pointer_{-1.0f};
    bool canvasPointer_ = false;
    bool rejected_ = false;
    mutable bool dirty_ = true;
};
