#pragma once

#include <GL/glew.h>
#include <GL/glm/glm.hpp>

#include <span>

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 color;
};

class BoxRenderer {
public:
    BoxRenderer() = default;
    BoxRenderer(const BoxRenderer&) = delete;
    BoxRenderer& operator=(const BoxRenderer&) = delete;
    ~BoxRenderer();

    bool initialize(std::span<const MeshVertex> staticMesh = {}, std::span<const MeshVertex> movableMesh = {});
    void begin(const glm::mat4& viewProjection) const;
    void drawBox(const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint) const;
    void drawBox(const glm::mat4& model, const glm::vec3& tint) const;
    void drawGrid(float halfSize) const;
    void drawStaticMesh() const;
    void drawMovableMesh(const glm::mat4& model, const glm::vec3& tint) const;

private:
    void drawMesh(GLint first, GLsizei count, const glm::mat4& model, const glm::vec3& tint) const;

    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    GLuint vertexBuffer_ = 0;
    GLint viewProjectionLocation_ = -1;
    GLint modelLocation_ = -1;
    GLint cubeTintLocation_ = -1;
    GLint gridHalfSizeLocation_ = -1;
    GLint useVertexColorLocation_ = -1;
    GLsizei staticMeshCount_ = 0;
    GLsizei movableMeshCount_ = 0;
};
