#pragma once

#include "../shared/BoxRenderer.h"

#include <optional>

class TestRoom {
public:
    enum class ExitState { Locked, Open, Escaped };

    TestRoom() = default;
    TestRoom(const TestRoom&) = delete;
    TestRoom& operator=(const TestRoom&) = delete;

    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
    glm::vec3 resolveMovement(const glm::vec3& previous, const glm::vec3& desired);
    ExitState exitState() const;
    void draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const;

    static constexpr float halfSize = 20.0f;

private:
    struct Cube {
        glm::vec3 center;
        float size;
    };

    struct Placement {
        Cube cube;
        bool inReturnBox;
    };

    std::optional<Placement> placement(const glm::vec3& eye, const glm::vec3& direction) const;

    Cube cube_{{0.0f, 0.5f, -4.0f}, 1.0f};
    float heldSizePerDepth_ = 0.0f;
    ExitState exitState_ = ExitState::Locked;
    BoxRenderer renderer_;
};
