#include "TestRoom.h"

#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
constexpr char vertexSource[] = R"(#version 330 core
layout (location = 0) in vec3 position;
uniform mat4 viewProjection;
uniform mat4 model;
out vec3 localPosition;
void main() {
    localPosition = position;
    gl_Position = viewProjection * model * vec4(position, 1.0);
}
)";

constexpr char fragmentSource[] = R"(#version 330 core
in vec3 localPosition;
uniform vec3 cubeTint;
out vec4 pixel;
void main() {
    if (cubeTint.r + cubeTint.g + cubeTint.b > 0.0) {
        vec3 normal = abs(normalize(cross(dFdx(localPosition), dFdy(localPosition))));
        float light = dot(normal, vec3(0.80, 1.0, 0.64));
        vec3 nearEdge = step(vec3(0.47), abs(localPosition));
        float outline = step(2.0, nearEdge.x + nearEdge.y + nearEdge.z);
        pixel = vec4(cubeTint * light * mix(1.0, 0.45, outline), 1.0);
        return;
    }
    vec2 floorPosition = localPosition.xz;
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
constexpr float playerRadius = 0.25f;
constexpr float minimumCubeSize = 0.15f;
constexpr glm::vec3 returnBoxCenter{1.45f, 0.0f, -2.65f};
constexpr float returnBoxWidth = 0.84f;
constexpr float returnBoxWall = 0.08f;
constexpr float returnBoxOpening = returnBoxWidth - 2.0f * returnBoxWall;
constexpr float returnBoxFloor = 0.10f;
constexpr float returnBoxRim = 0.65f;
constexpr float maxReturnCubeSize = returnBoxOpening - 0.04f;
constexpr float doorZ = -7.0f;
constexpr float doorHalfWidth = 1.0f;
constexpr float doorHeight = 3.1f;
constexpr float wallHeight = 4.0f;
constexpr float wallDepth = 0.30f;
constexpr float doorDepth = 0.16f;
constexpr glm::vec3 openDoorCenter{-doorHalfWidth - doorDepth * 0.5f, doorHeight * 0.5f, doorZ - doorHalfWidth};
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

std::optional<float> rayBoxDistance(const glm::vec3& eye, const glm::vec3& direction,
                                    const glm::vec3& center, const glm::vec3& half) {
    float enter = 0.0f;
    float leave = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        const float minOffset = center[axis] - half[axis] - eye[axis];
        const float maxOffset = center[axis] + half[axis] - eye[axis];
        if (std::abs(direction[axis]) < 0.0001f) {
            if (minOffset > 0.0f || maxOffset < 0.0f) {
                return std::nullopt;
            }
            continue;
        }
        const float a = minOffset / direction[axis];
        const float b = maxOffset / direction[axis];
        enter = std::max(enter, std::min(a, b));
        leave = std::min(leave, std::max(a, b));
        if (enter > leave) {
            return std::nullopt;
        }
    }
    return enter;
}

bool insideSquare(const glm::vec3& position, const glm::vec3& center, float halfWidth) {
    return std::abs(position.x - center.x) <= halfWidth &&
           std::abs(position.z - center.z) <= halfWidth;
}
}

TestRoom::~TestRoom() {
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

bool TestRoom::initialize() {
    if (program_) {
        return true;
    }

    program_ = makeRoomProgram();
    if (!program_) {
        return false;
    }

    std::array<float, floorVertexCount * 3 + std::size(cubeIndices) * 3> vertices{
        -halfSize, 0.0f, -halfSize,
        -halfSize, 0.0f,  halfSize,
         halfSize, 0.0f,  halfSize,
        -halfSize, 0.0f, -halfSize,
         halfSize, 0.0f,  halfSize,
         halfSize, 0.0f, -halfSize,
    };
    auto next = vertices.begin() + floorVertexCount * 3;
    for (unsigned int index : cubeIndices) {
        const auto& corner = cubeCorners[index];
        *next++ = corner.x;
        *next++ = corner.y;
        *next++ = corner.z;
    }
    glGenVertexArrays(1, &vertexArray_);
    glBindVertexArray(vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    viewProjectionLocation_ = glGetUniformLocation(program_, "viewProjection");
    modelLocation_ = glGetUniformLocation(program_, "model");
    cubeTintLocation_ = glGetUniformLocation(program_, "cubeTint");
    return true;
}

std::optional<TestRoom::Placement> TestRoom::placement(const glm::vec3& eye, const glm::vec3& direction) const {
    if (direction.y >= -0.001f) {
        return std::nullopt;
    }

    const float rimDepth = (returnBoxRim - eye.y) / direction.y;
    const glm::vec3 rimPoint = eye + direction * rimDepth;
    if (rimDepth > 0.0f && insideSquare(rimPoint, returnBoxCenter, returnBoxWidth * 0.5f)) {
        const glm::vec3 binFloor = returnBoxCenter + glm::vec3{0.0f, returnBoxFloor, 0.0f};
        const float baseDepth = glm::dot(binFloor - eye, direction);
        const float size = heldSizePerDepth_ * baseDepth / (1.0f - direction.y * heldSizePerDepth_ * 0.5f);
        if (!insideSquare(rimPoint, returnBoxCenter, returnBoxOpening * 0.5f) ||
            size < minimumCubeSize || size > maxReturnCubeSize) {
            return std::nullopt;
        }
        return Placement{{binFloor + glm::vec3{0.0f, size * 0.5f, 0.0f}, size}, true};
    }

    // The center follows the sight ray, and its height must equal half the scaled side length.
    const float depth = eye.y / (0.5f * heldSizePerDepth_ - direction.y);
    const float size = heldSizePerDepth_ * depth;
    const float half = size * 0.5f;
    const glm::vec3 center = eye + direction * depth;
    if (size < minimumCubeSize || insideSquare(center, eye, half + playerRadius) ||
        insideSquare(center, returnBoxCenter, half + returnBoxWidth * 0.5f) ||
        std::abs(center.x) + half > halfSize || center.z + half > halfSize ||
        center.z - half <= doorZ + wallDepth * 0.5f) {
        return std::nullopt;
    }
    return Placement{{{center.x, half, center.z}, size}, false};
}

void TestRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
    if (exitState_ != ExitState::Locked) {
        return;
    }
    if (heldSizePerDepth_ > 0.0f) {
        if (const auto target = placement(eye, direction)) {
            cube_ = target->cube;
            heldSizePerDepth_ = 0.0f;
            if (target->inReturnBox) {
                exitState_ = ExitState::Open;
            }
        }
        return;
    }

    const auto hit = rayBoxDistance(eye, direction, cube_.center, glm::vec3(cube_.size * 0.5f));
    const auto binHit = rayBoxDistance(
        eye, direction, returnBoxCenter + glm::vec3{0.0f, returnBoxRim * 0.5f, 0.0f},
        {returnBoxWidth * 0.5f, returnBoxRim * 0.5f, returnBoxWidth * 0.5f});
    if (hit && *hit > 0.05f && (!binHit || *hit < *binHit)) {
        const float depth = glm::dot(cube_.center - eye, direction);
        // Limit the apparent size when grabbing right next to the cube.
        heldSizePerDepth_ = cube_.size / std::max(depth, cube_.size);
    }
}

glm::vec3 TestRoom::resolveMovement(const glm::vec3& previous, const glm::vec3& desired) {
    const auto blocked = [this](const glm::vec3& position) {
        const bool atWall = std::abs(position.z - doorZ) <= wallDepth * 0.5f + playerRadius;
        const bool locked = exitState_ == ExitState::Locked;
        if (atWall && (locked || std::abs(position.x) > doorHalfWidth - playerRadius)) {
            return true;
        }
        const bool atOpenDoor = std::abs(position.x - openDoorCenter.x) < doorDepth * 0.5f + playerRadius &&
                                std::abs(position.z - openDoorCenter.z) < doorHalfWidth + playerRadius;
        return (!locked && atOpenDoor) ||
               insideSquare(position, returnBoxCenter, returnBoxWidth * 0.5f + playerRadius);
    };

    glm::vec3 position{desired.x, desired.y, previous.z};
    if (blocked(position)) {
        position.x = previous.x;
    }
    position.z = desired.z;
    if (blocked(position)) {
        position.z = previous.z;
    }
    if (exitState_ == ExitState::Open && position.z < doorZ - wallDepth * 0.5f - playerRadius) {
        exitState_ = ExitState::Escaped;
    }
    return position;
}

TestRoom::ExitState TestRoom::exitState() const {
    return exitState_;
}

void TestRoom::draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const {
    glUseProgram(program_);
    glBindVertexArray(vertexArray_);
    glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, glm::value_ptr(viewProjection));
    const glm::mat4 identity{1.0f};
    glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(identity));
    glUniform3f(cubeTintLocation_, 0.0f, 0.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, floorVertexCount);

    const auto drawBox = [this, &identity](const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint) {
        const glm::mat4 model = glm::scale(glm::translate(identity, center), size);
        glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(cubeTintLocation_, 1, glm::value_ptr(tint));
        glDrawArrays(GL_TRIANGLES, floorVertexCount, static_cast<GLsizei>(std::size(cubeIndices)));
    };

    const glm::vec3 wallTint{0.42f, 0.49f, 0.56f};
    const float sideCenter = (halfSize + doorHalfWidth) * 0.5f;
    const glm::vec3 sideSize{halfSize - doorHalfWidth, wallHeight, wallDepth};
    drawBox({-sideCenter, wallHeight * 0.5f, doorZ}, sideSize, wallTint);
    drawBox({sideCenter, wallHeight * 0.5f, doorZ}, sideSize, wallTint);
    drawBox({0.0f, (wallHeight + doorHeight) * 0.5f, doorZ},
            {doorHalfWidth * 2.0f, wallHeight - doorHeight, wallDepth}, wallTint);

    const bool locked = exitState_ == ExitState::Locked;
    const glm::vec3 doorTint{0.23f, 0.54f, 0.74f};
    if (locked) {
        drawBox({0.0f, doorHeight * 0.5f, doorZ}, {doorHalfWidth * 2.0f, doorHeight, doorDepth}, doorTint);
    } else {
        drawBox(openDoorCenter, {doorDepth, doorHeight, doorHalfWidth * 2.0f}, doorTint);
    }
    const glm::vec3 indicator = locked ? glm::vec3{0.95f, 0.29f, 0.24f} : glm::vec3{0.30f, 0.95f, 0.46f};
    drawBox({0.0f, doorHeight + 0.30f, doorZ + wallDepth * 0.5f + 0.03f},
            {0.50f, 0.15f, 0.06f}, indicator);

    const glm::vec3 binTint{0.84f, 0.77f, 0.34f};
    drawBox(returnBoxCenter + glm::vec3{0.0f, returnBoxFloor * 0.5f, 0.0f},
            {returnBoxWidth, returnBoxFloor, returnBoxWidth}, {0.47f, 0.43f, 0.22f});
    const float binWallOffset = (returnBoxWidth - returnBoxWall) * 0.5f;
    const float binWallY = (returnBoxRim + returnBoxFloor) * 0.5f;
    const float binWallHeight = returnBoxRim - returnBoxFloor;
    for (float side : {-binWallOffset, binWallOffset}) {
        drawBox(returnBoxCenter + glm::vec3{side, binWallY, 0.0f},
                {returnBoxWall, binWallHeight, returnBoxWidth}, binTint);
        drawBox(returnBoxCenter + glm::vec3{0.0f, binWallY, side},
                {returnBoxOpening, binWallHeight, returnBoxWall}, binTint);
    }

    Cube displayed = cube_;
    glm::vec3 tint{0.98f, 0.61f, 0.24f};
    if (heldSizePerDepth_ > 0.0f) {
        if (const auto target = placement(eye, direction)) {
            displayed = target->cube;
            tint = {0.32f, 0.88f, 0.77f};
        } else {
            constexpr float carryDepth = 2.0f;
            displayed = {eye + direction * carryDepth, std::min(heldSizePerDepth_ * carryDepth, 1.5f)};
            tint = {0.94f, 0.45f, 0.36f};
        }
    }
    drawBox(displayed.center, glm::vec3(displayed.size), tint);
}
