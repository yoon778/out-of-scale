#pragma once

#include <GL/glew.h>
#include <GL/glm/glm.hpp>

#include <array>
#include <span>
#include <vector>

// Adaptations of the student's coursework; no world-item spawning.
class CourseworkLabs {
public:
    CourseworkLabs();
    CourseworkLabs(const CourseworkLabs&) = delete;
    CourseworkLabs& operator=(const CourseworkLabs&) = delete;
    ~CourseworkLabs();
    bool initialize();
    void select(int lesson);
    int selected() const;
    void key(int key, int action, int mods);
    void pointer(glm::vec2 ndc, bool inside);
    void mouse(int button, int action);
    void scroll(double amount);
    bool update(float elapsed);
    void draw() const;
    bool keyReady() const;
    bool keyTaken() const;
    void consumeKey();
    void clearInput();

private:
    struct Vertex { glm::vec3 position; glm::vec3 color; glm::vec3 normal{0,1,0}; };
    struct Shape2d { int type; glm::vec2 center; glm::vec3 color; };
    struct Part { glm::vec2 center; glm::vec2 size; float angle; float scale; };
    void createShape(int type);
    void moveShape(int index, glm::vec2 delta);
    void resetStack();
    void resetTransform();
    void clampPart(Part& part);
    void snapKeyPart(int index);
    void render(std::span<const Vertex> vertices, GLenum mode, const glm::mat4& model,
                const glm::mat4& viewProjection, bool lit = false) const;
    void rect(glm::vec2 center, glm::vec2 size, glm::vec3 color,
              const glm::mat4& model = glm::mat4{1.0f}, bool line = false) const;
    void drawShapes() const;
    void drawStack() const;
    void drawTransform() const;
    void drawSolar() const;
    void drawLighting() const;
    void drawAssembly() const;

    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    GLuint vertexBuffer_ = 0;
    std::vector<Vertex> sphere_;
    std::vector<Vertex> cube_;
    std::vector<Vertex> pyramid_;
    std::vector<Vertex> coloredCube_;
    std::vector<Vertex> coloredPyramid_;
    int lesson_ = 0;
    std::array<bool, 349> pressed_{};
    glm::vec2 pointer_{0.0f};
    glm::vec2 dragOffset_{0.0f};
    bool pointerInside_ = false;
    bool dragging_ = false;
    std::vector<Shape2d> shapes_;
    int shapeSelection_ = -1;
    unsigned random_ = 0x31415926u;
    std::array<float, 2> stackY_{{.5f,-.7f}};
    std::array<float, 2> stackDirection_{{-1,1}};
    int stackCount_ = 0;
    float stackTravel_ = -1;
    float stackMatchY_ = 0;
    glm::vec2 transformMove_{0};
    glm::vec2 transformAngle_{-30,-30};
    glm::vec2 transformDirection_{0};
    bool transformCube_ = true;
    bool transformWire_ = false;
    bool transformDepth_ = true;
    glm::vec3 solarMove_{0};
    std::array<float, 3> planetAngle_{{0,120,240}};
    std::array<float, 3> moonAngle_{{40,130,220}};
    float moonRotation_ = 0;
    float orbitScale_ = 1;
    float solarTime_ = 0;
    bool solarLarge_ = false;
    bool solarPulse_ = false;
    bool solarMoon_ = false;
    bool solarPerspective_ = true;
    bool solarWire_ = false;
    bool lightingCube_ = true;
    bool lightingEnabled_ = true;
    bool lightingSpin_ = false;
    int lightOrbit_ = 0;
    float lightAngle_ = 43;
    float objectAngle_ = -25;
    float lightRadius_ = 2;
    std::array<Part, 4> keyParts_;
    int partSelection_ = -1;
    bool keyTaken_ = false;
};
