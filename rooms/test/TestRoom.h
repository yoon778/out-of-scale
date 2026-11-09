#pragma once

#include <GL/glew.h>
#include <GL/glm/glm.hpp>

class TestRoom {
public:
    TestRoom() = default;
    TestRoom(const TestRoom&) = delete;
    TestRoom& operator=(const TestRoom&) = delete;
    ~TestRoom();

    bool initialize();
    void draw(const glm::mat4& viewProjection) const;

    static constexpr float halfSize = 20.0f;

private:
    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    GLuint vertexBuffer_ = 0;
    GLint viewProjectionLocation_ = -1;
};
