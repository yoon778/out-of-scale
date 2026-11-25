#pragma once

#include "../shared/BoxRenderer.h"
#include "CourseworkLabs.h"

#include <array>
#include <optional>

class ComputerScreen {
public:
    enum class Action { None, TakeCompleteKey };
    ComputerScreen() = default;
    ComputerScreen(const ComputerScreen&) = delete;
    ComputerScreen& operator=(const ComputerScreen&) = delete;
    ~ComputerScreen();

    bool initialize();
    void keyInput(int key, int action, int mods);
    Action click(float u, float v); // Monitor UV, top-left origin.
    static std::optional<glm::vec2> rayToUv(const glm::vec3& eye,
        const glm::vec3& direction, float maxDistance);
    void update(float elapsed);
    void selectLesson(int zeroBased);
    int selectedLesson() const;
    void consumeKey(); // Only after the world successfully receives the key.
    bool keyTaken() const;
    void draw(const glm::mat4& viewProjection) const;
    void drawCube(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawKeyPiece(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawCompleteKey(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void clearInput();
    static constexpr glm::vec3 center{5.505f, 1.85f, 3.0f};

private:
    enum class View { Ide, Desktop, KeyDocument };
    void setView(View view);
    void renderTexture() const;
    void drawTexture(GLuint texture, const glm::mat4& transform, bool flip) const;

    BoxRenderer renderer_;
    CourseworkLabs labs_;
    GLuint framebuffer_ = 0;
    GLuint color_ = 0;
    GLuint depth_ = 0;
    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    std::array<GLuint, 5> workspaceLabels_{};
    GLuint desktopLabel_ = 0;
    GLuint keyDocumentLabel_ = 0;
    GLuint emptyDocumentLabel_ = 0;
    GLint transform_ = -1;
    GLint flip_ = -1;
    View view_ = View::Ide;
    bool keyTaken_ = false;
    mutable bool dirty_ = true;
};
