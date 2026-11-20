#pragma once

#include "../shared/BoxRenderer.h"

class ComputerScreen {
public:
    enum class Item { Cube, Key };
    enum class Action { None, TakeCube, TakeKey };
    enum class CarryHint { None, Common, CompleteKey, Socket, Miniature };
    ComputerScreen() = default;
    ComputerScreen(const ComputerScreen&) = delete;
    ComputerScreen& operator=(const ComputerScreen&) = delete;
    ~ComputerScreen();

    bool initialize();
    void input(float horizontal, float vertical, float turn, float elapsed);
    void scroll(double amount);
    void setCubeVisible(bool visible);
    void setCube(float edge, float yaw);
    void selectItem(int number);
    Item selection() const;
    Action confirm();
    bool cubeSolved() const;
    bool keySolved() const;
    float keyYaw() const;
    void setKeyVisible(bool visible);
    void setKey(float yaw);
    float edge() const;
    float yaw() const;
    void draw(const glm::mat4& viewProjection) const;
    void drawCube(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawKeyPiece(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawCompleteKey(const glm::mat4& viewProjection, const glm::mat4& model) const;
    void drawGuide(bool active, bool holding, bool holdingKey = false) const;
    void drawCarryGuide(CarryHint hint) const;

    static constexpr glm::vec3 center{5.505f, 1.85f, 3.0f};

private:
    void clampPosition();
    void renderTexture() const;
    void drawTexture(GLuint texture, const glm::mat4& transform, bool flip) const;
    void drawLabel(GLuint texture) const;

    BoxRenderer renderer_;
    GLuint framebuffer_ = 0;
    GLuint color_ = 0;
    GLuint depth_ = 0;
    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    GLuint guide_ = 0;
    GLuint holdingGuide_ = 0;
    GLuint cubeGuide_ = 0;
    GLuint keyGuide_ = 0;
    GLuint rejectedGuide_ = 0;
    GLuint outsideGuide_ = 0;
    GLuint completeKeyGuide_ = 0;
    GLuint socketGuide_ = 0;
    GLuint miniatureGuide_ = 0;
    GLint transform_ = -1;
    GLint flip_ = -1;
    glm::vec2 position_{-0.55f, -0.20f};
    float edge_ = 0.40f;
    float yaw_ = 0.35f;
    bool visible_ = true;
    Item selected_ = Item::Cube;
    bool cubeSolved_ = false;
    bool keySolved_ = false;
    bool keyVisible_ = true;
    float keyYaw_ = 1.35f;
    bool rejected_ = false;
    mutable bool dirty_ = true;
};
