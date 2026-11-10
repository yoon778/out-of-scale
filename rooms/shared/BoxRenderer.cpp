#include "BoxRenderer.h"

#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <vector>

namespace {
constexpr char vertexSource[] = R"(#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
uniform mat4 viewProjection;
uniform mat4 model;
out vec3 localPosition;
out vec3 vertexColor;
void main() {
    localPosition = position;
    vertexColor = color;
    gl_Position = viewProjection * model * vec4(position, 1.0);
}
)";

constexpr char fragmentSource[] = R"(#version 330 core
in vec3 localPosition;
in vec3 vertexColor;
uniform vec3 cubeTint;
uniform float gridHalfSize;
uniform bool useVertexColor;
out vec4 pixel;
void main() {
    if (useVertexColor) {
        pixel = vec4(vertexColor * cubeTint, 1.0);
        return;
    }
    if (gridHalfSize <= 0.0) {
        vec3 normal = abs(normalize(cross(dFdx(localPosition), dFdy(localPosition))));
        float light = dot(normal, vec3(0.80, 1.0, 0.64));
        vec3 nearEdge = step(vec3(0.47), abs(localPosition));
        float outline = step(2.0, nearEdge.x + nearEdge.y + nearEdge.z);
        pixel = vec4(cubeTint * light * mix(1.0, 0.45, outline), 1.0);
        return;
    }
    vec2 floorPosition = localPosition.xz * gridHalfSize;
    vec2 edgeDistance = abs(fract(floorPosition + 0.5) - 0.5);
    vec2 aa = max(fwidth(floorPosition), vec2(0.0001));
    vec2 line = 1.0 - smoothstep(vec2(0.015), vec2(0.015) + aa, edgeDistance);
    float checker = mod(floor(floorPosition.x) + floor(floorPosition.y), 2.0);
    vec3 color = mix(vec3(0.17, 0.23, 0.29), vec3(0.22, 0.30, 0.36), checker);
    color = mix(color, vec3(0.44, 0.56, 0.62), max(line.x, line.y));
    pixel = vec4(color, 1.0);
}
)";

GLuint compileShader(GLenum kind, const char* source) {
    const GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    char error[1024]{};
    glGetShaderInfoLog(shader, sizeof(error), nullptr, error);
    std::cerr << error << '\n';
    glDeleteShader(shader);
    return 0;
}

GLuint makeRoomProgram() {
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = 0;
    if (vertex && fragment) {
        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            char error[1024]{};
            glGetProgramInfoLog(program, sizeof(error), nullptr, error);
            std::cerr << error << '\n';
            glDeleteProgram(program);
            program = 0;
        }
    }
    if (vertex) {
        glDeleteShader(vertex);
    }
    if (fragment) {
        glDeleteShader(fragment);
    }
    return program;
}

constexpr GLsizei floorVertexCount = 6;
constexpr glm::vec3 cubeCorners[] = {
    {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
    { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
    {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
    { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
};
constexpr unsigned int cubeIndices[] = {
    0, 3, 2, 0, 2, 1,
    4, 5, 6, 4, 6, 7,
    0, 4, 7, 0, 7, 3,
    1, 2, 6, 1, 6, 5,
    0, 1, 5, 0, 5, 4,
    3, 7, 6, 3, 6, 2,
};
constexpr GLsizei cubeVertexCount = static_cast<GLsizei>(std::size(cubeIndices));
constexpr GLsizei primitiveVertexCount = floorVertexCount + cubeVertexCount;

void enableMeshAttributes() {
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, color)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
}
}

BoxRenderer::~BoxRenderer() {
    if (flatPolygonBuffer_) {
        glDeleteBuffers(1, &flatPolygonBuffer_);
    }
    if (flatPolygonArray_) {
        glDeleteVertexArrays(1, &flatPolygonArray_);
    }
    if (vertexBuffer_) {
        glDeleteBuffers(1, &vertexBuffer_);
    }
    if (vertexArray_) {
        glDeleteVertexArrays(1, &vertexArray_);
    }
    if (program_) {
        glDeleteProgram(program_);
    }
}

bool BoxRenderer::initialize(std::span<const MeshVertex> staticMesh, std::span<const MeshVertex> movableMesh,
                             std::span<const MeshVertex> moonMesh) {
    if (program_) {
        return true;
    }

    program_ = makeRoomProgram();
    if (!program_) {
        return false;
    }

    std::array<MeshVertex, primitiveVertexCount> vertices{{
        {{-1.0f, 0.0f, -1.0f}, {}},
        {{-1.0f, 0.0f,  1.0f}, {}},
        {{ 1.0f, 0.0f,  1.0f}, {}},
        {{-1.0f, 0.0f, -1.0f}, {}},
        {{ 1.0f, 0.0f,  1.0f}, {}},
        {{ 1.0f, 0.0f, -1.0f}, {}},
    }};
    auto next = vertices.begin() + floorVertexCount;
    for (unsigned int index : cubeIndices) {
        (next++)->position = cubeCorners[index];
    }
    glGenVertexArrays(1, &vertexArray_);
    glBindVertexArray(vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    constexpr auto primitiveBytes = primitiveVertexCount * sizeof(MeshVertex);
    const auto movableOffset = primitiveBytes + staticMesh.size_bytes();
    const auto moonOffset = movableOffset + movableMesh.size_bytes();
    glBufferData(GL_ARRAY_BUFFER, moonOffset + moonMesh.size_bytes(), nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, primitiveBytes, vertices.data());
    if (!staticMesh.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, primitiveBytes, staticMesh.size_bytes(), staticMesh.data());
    }
    if (!movableMesh.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, movableOffset, movableMesh.size_bytes(), movableMesh.data());
    }
    if (!moonMesh.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, moonOffset, moonMesh.size_bytes(), moonMesh.data());
    }
    enableMeshAttributes();

    glGenVertexArrays(1, &flatPolygonArray_);
    glBindVertexArray(flatPolygonArray_);
    glGenBuffers(1, &flatPolygonBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, flatPolygonBuffer_);
    enableMeshAttributes();
    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    staticMeshCount_ = static_cast<GLsizei>(staticMesh.size());
    movableMeshCount_ = static_cast<GLsizei>(movableMesh.size());
    moonMeshCount_ = static_cast<GLsizei>(moonMesh.size());
    viewProjectionLocation_ = glGetUniformLocation(program_, "viewProjection");
    modelLocation_ = glGetUniformLocation(program_, "model");
    cubeTintLocation_ = glGetUniformLocation(program_, "cubeTint");
    gridHalfSizeLocation_ = glGetUniformLocation(program_, "gridHalfSize");
    useVertexColorLocation_ = glGetUniformLocation(program_, "useVertexColor");
    return true;
}

void BoxRenderer::begin(const glm::mat4& viewProjection) const {
    glUseProgram(program_);
    glBindVertexArray(vertexArray_);
    glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, glm::value_ptr(viewProjection));
}

void BoxRenderer::drawBox(const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint) const {
    const glm::mat4 model = glm::scale(glm::translate(glm::mat4{1.0f}, center), size);
    drawBox(model, tint);
}

void BoxRenderer::drawBox(const glm::mat4& model, const glm::vec3& tint) const {
    glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(cubeTintLocation_, 1, glm::value_ptr(tint));
    glUniform1f(gridHalfSizeLocation_, 0.0f);
    glUniform1i(useVertexColorLocation_, GL_FALSE);
    glDrawArrays(GL_TRIANGLES, floorVertexCount, cubeVertexCount);
}

void BoxRenderer::drawGrid(float halfSize) const {
    const glm::mat4 model = glm::scale(glm::mat4{1.0f}, {halfSize, 1.0f, halfSize});
    glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1f(gridHalfSizeLocation_, halfSize);
    glUniform1i(useVertexColorLocation_, GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, floorVertexCount);
}

void BoxRenderer::drawStaticMesh() const {
    drawMesh(primitiveVertexCount, staticMeshCount_, glm::mat4{1.0f}, glm::vec3{1.0f});
}

void BoxRenderer::drawMovableMesh(const glm::mat4& model, const glm::vec3& tint) const {
    drawMesh(primitiveVertexCount + staticMeshCount_, movableMeshCount_, model, tint);
}

void BoxRenderer::drawMoonMesh(const glm::mat4& model, const glm::vec3& tint) const {
    drawMesh(primitiveVertexCount + staticMeshCount_ + movableMeshCount_, moonMeshCount_, model, tint);
}

void BoxRenderer::drawFlatPolygon(std::span<const glm::vec2> outline, float height, const glm::vec3& color) const {
    if (outline.size() < 3) {
        return;
    }

    std::vector<MeshVertex> vertices;
    vertices.reserve(outline.size());
    for (const auto& point : outline) {
        vertices.push_back({{point.x, height, point.y}, color});
    }
    glBindVertexArray(flatPolygonArray_);
    glBindBuffer(GL_ARRAY_BUFFER, flatPolygonBuffer_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex), vertices.data(), GL_STREAM_DRAW);
    drawMesh(0, static_cast<GLsizei>(vertices.size()), glm::mat4{1.0f}, glm::vec3{1.0f}, GL_TRIANGLE_FAN);
    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
}

void BoxRenderer::drawMesh(GLint first, GLsizei count, const glm::mat4& model, const glm::vec3& tint,
                           GLenum primitive) const {
    glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(cubeTintLocation_, 1, glm::value_ptr(tint));
    glUniform1i(useVertexColorLocation_, GL_TRUE);
    glDrawArrays(primitive, first, count);
}
