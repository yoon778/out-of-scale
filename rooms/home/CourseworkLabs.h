#pragma once

#include <GL/glew.h>
#include <GL/glm/glm.hpp>

#include <array>
#include <span>
#include <vector>

// Five coursework scenes with arrow-key translation only.
class CourseworkLabs {
public:
    CourseworkLabs() = default;
    CourseworkLabs(const CourseworkLabs&) = delete;
    CourseworkLabs& operator=(const CourseworkLabs&) = delete;
    ~CourseworkLabs();
    bool initialize();
    void select(int lesson);
    int selected() const;
    void key(int key, int action, int mods);
    bool update(float elapsed);
    void draw() const;
    void clearInput();

private:
    struct Vertex { glm::vec3 position; glm::vec3 color; glm::vec3 normal{0,1,0}; };
    struct Shape2d { int type; glm::vec2 center; glm::vec3 color; };
    void render(std::span<const Vertex> vertices, GLenum mode, const glm::mat4& model,
                const glm::mat4& viewProjection, bool lit = false) const;
    void rect(glm::vec2 center, glm::vec2 size, glm::vec3 color,
              const glm::mat4& model = glm::mat4{1.0f}, bool line = false) const;
    void drawShapes() const;
    void drawStack() const;
    void drawTransform() const;
    void drawSolar() const;
    void drawLighting() const;

    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    GLuint vertexBuffer_ = 0;
    std::vector<Vertex> sphere_;
    std::vector<Vertex> cube_;
    std::vector<Vertex> coloredCube_;
    int lesson_ = 0;
    std::array<bool, 4> arrows_{};
    std::array<glm::vec2, 5> lessonOffsets_{{{0,0},{0,0},{0,0},{0,0},{0,0}}};
};
