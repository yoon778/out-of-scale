#include "TestRoom.h"

#include <GL/glm/gtc/type_ptr.hpp>
#include <iostream>

namespace {
constexpr char vertexSource[] = R"(#version 330 core
layout (location = 0) in vec3 position;
uniform mat4 viewProjection;
out vec2 floorPosition;
void main() {
    floorPosition = position.xz;
    gl_Position = viewProjection * vec4(position, 1.0);
}
)";

constexpr char fragmentSource[] = R"(#version 330 core
in vec2 floorPosition;
out vec4 pixel;
void main() {
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

GLuint makeFloorProgram() {
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
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
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

    program_ = makeFloorProgram();
    if (!program_) {
        return false;
    }

    constexpr float vertices[] = {
        -halfSize, 0.0f, -halfSize,
        -halfSize, 0.0f,  halfSize,
         halfSize, 0.0f,  halfSize,
        -halfSize, 0.0f, -halfSize,
         halfSize, 0.0f,  halfSize,
         halfSize, 0.0f, -halfSize,
    };
    glGenVertexArrays(1, &vertexArray_);
    glBindVertexArray(vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    viewProjectionLocation_ = glGetUniformLocation(program_, "viewProjection");
    return true;
}

void TestRoom::draw(const glm::mat4& viewProjection) const {
    glUseProgram(program_);
    glBindVertexArray(vertexArray_);
    glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
