#include "ComputerScreen.h"

#include <windows.h>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int textureWidth = 640;
constexpr int textureHeight = 360;
constexpr int guideWidth = 860;
constexpr int guideHeight = 60;

class GlState {
public:
    GlState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer_);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer_);
        glGetIntegerv(GL_VIEWPORT, viewport_);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor_);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clearDepth_);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunction_);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask_);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray_);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer_);
        glGetIntegerv(GL_POLYGON_MODE, polygonMode_);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0_);
        glActiveTexture(activeTexture_);
        depth_ = glIsEnabled(GL_DEPTH_TEST);
        scissor_ = glIsEnabled(GL_SCISSOR_TEST);
    }

    ~GlState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer_);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer_);
        glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
        glClearColor(clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]);
        glClearDepth(clearDepth_);
        glDepthFunc(depthFunction_);
        glDepthMask(depthMask_);
        restore(GL_DEPTH_TEST, depth_);
        restore(GL_SCISSOR_TEST, scissor_);
        // OpenGL's core context shares the polygon mode between both faces.
        glPolygonMode(GL_FRONT_AND_BACK, polygonMode_[0]);
        glUseProgram(program_);
        glBindVertexArray(vertexArray_);
        glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer_);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture0_);
        glActiveTexture(activeTexture_);
    }

private:
    static void restore(GLenum capability, GLboolean enabled) {
        if (enabled) glEnable(capability);
        else glDisable(capability);
    }

    GLint drawFramebuffer_ = 0;
    GLint readFramebuffer_ = 0;
    GLint viewport_[4]{};
    GLfloat clearColor_[4]{};
    GLdouble clearDepth_ = 1.0;
    GLint depthFunction_ = GL_LESS;
    GLboolean depthMask_ = GL_TRUE;
    GLint program_ = 0;
    GLint vertexArray_ = 0;
    GLint arrayBuffer_ = 0;
    GLint renderbuffer_ = 0;
    GLint polygonMode_[2]{};
    GLint activeTexture_ = GL_TEXTURE0;
    GLint texture0_ = 0;
    GLboolean depth_ = GL_FALSE;
    GLboolean scissor_ = GL_FALSE;
};

GLuint shader(GLenum type, const char* source) {
    const GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint valid = GL_FALSE;
    glGetShaderiv(result, GL_COMPILE_STATUS, &valid);
    if (valid != GL_TRUE) {
        glDeleteShader(result);
        return 0;
    }
    return result;
}

GLuint guideTexture(const wchar_t* text) {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return 0;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = guideWidth;
    info.bmiHeader.biHeight = -guideHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) {
        DeleteDC(dc);
        return 0;
    }
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    RECT rect{0, 0, guideWidth, guideHeight};
    HBRUSH brush = CreateSolidBrush(RGB(23, 35, 42));
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
    HFONT font = CreateFontW(-23, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(240, 245, 244));
    DrawTextW(dc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();
    GLuint result = 0;
    glGenTextures(1, &result);
    glBindTexture(GL_TEXTURE_2D, result);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, guideWidth, guideHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return result;
}

const std::array<MeshVertex, 36>& cubeMesh() {
    static const auto vertices = [] {
        std::array<MeshVertex, 36> result{};
        constexpr std::array<glm::vec3, 8> points{{
            {-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
            {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}}};
        constexpr int faces[6][6]{{0,2,1,0,3,2},{4,5,6,4,6,7},{0,4,7,0,7,3},
                                 {1,2,6,1,6,5},{3,7,6,3,6,2},{0,1,5,0,5,4}};
        constexpr glm::vec3 colors[6]{{.16f,.37f,.48f},{.34f,.74f,.85f},{.23f,.51f,.65f},
                                     {.19f,.43f,.57f},{.66f,.89f,.89f},{.13f,.29f,.38f}};
        for (int face = 0; face < 6; ++face) {
            for (int index = 0; index < 6; ++index) {
                result[face * 6 + index] = {points[faces[face][index]], colors[face]};
            }
        }
        return result;
    }();
    return vertices;
}
} // namespace

ComputerScreen::~ComputerScreen() {
    if (framebuffer_) glDeleteFramebuffers(1, &framebuffer_);
    if (color_) glDeleteTextures(1, &color_);
    if (depth_) glDeleteRenderbuffers(1, &depth_);
    if (program_) glDeleteProgram(program_);
    if (vertexArray_) glDeleteVertexArrays(1, &vertexArray_);
    if (guide_) glDeleteTextures(1, &guide_);
    if (holdingGuide_) glDeleteTextures(1, &holdingGuide_);
}

bool ComputerScreen::initialize() {
    const GlState state;
    glActiveTexture(GL_TEXTURE0);
    if (!renderer_.initialize({}, cubeMesh())) return false;
    constexpr char vertex[] = R"(#version 330 core
uniform mat4 transform;
out vec2 uv;
void main() {
    const vec2 p[6] = vec2[6](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));
    uv = p[gl_VertexID];
    gl_Position = transform * vec4(uv - .5, 0, 1);
})";
    constexpr char fragment[] = R"(#version 330 core
uniform sampler2D image;
uniform bool flip;
in vec2 uv;
out vec4 pixel;
void main() { pixel = texture(image, vec2(uv.x, flip ? 1.0-uv.y : uv.y)); }
)";
    const GLuint vs = shader(GL_VERTEX_SHADER, vertex);
    const GLuint fs = shader(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) return false;
    transform_ = glGetUniformLocation(program_, "transform");
    flip_ = glGetUniformLocation(program_, "flip");
    glGenVertexArrays(1, &vertexArray_);
    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glGenTextures(1, &color_);
    glBindTexture(GL_TEXTURE_2D, color_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
    glGenRenderbuffers(1, &depth_);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, textureWidth, textureHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    guide_ = guideTexture(L"방향키 이동     Q / R 회전     휠 크기     E 꺼내기     Tab 돌아가기");
    holdingGuide_ = guideTexture(L"E 놓기     모니터를 보고 E 화면에 넣기");
    return complete && guide_ && holdingGuide_;
}

void ComputerScreen::input(float horizontal, float vertical, float turn, float elapsed) {
    if (horizontal == 0.0f && vertical == 0.0f && turn == 0.0f) return;
    position_ += glm::vec2{horizontal, vertical} * elapsed * .70f;
    yaw_ = std::remainder(yaw_ + turn * elapsed * 1.6f, 6.2831853f);
    clampPosition();
    dirty_ = true;
}

void ComputerScreen::scroll(double amount) {
    edge_ = std::clamp(edge_ * std::exp(-static_cast<float>(amount) * .12f), .40f, 1.0f);
    clampPosition();
    dirty_ = true;
}

void ComputerScreen::setCubeVisible(bool visible) { visible_ = visible; dirty_ = true; }
void ComputerScreen::setCube(float edge, float yaw) {
    edge_ = edge;
    yaw_ = yaw;
    clampPosition();
    visible_ = true;
    dirty_ = true;
}
float ComputerScreen::edge() const { return edge_; }
float ComputerScreen::yaw() const { return yaw_; }

void ComputerScreen::clampPosition() {
    const float rotatedExtent = std::abs(std::cos(yaw_)) + std::abs(std::sin(yaw_));
    const float halfWidth = edge_ * .5f * rotatedExtent;
    const float viewCosine = 4.0f / std::sqrt(17.0f);
    const float viewSine = 1.0f / std::sqrt(17.0f);
    const float halfHeight = edge_ * .5f * (viewCosine + viewSine * rotatedExtent);
    const float horizontalLimit = std::max(0.0f, std::min(.90f, 1.60f - halfWidth - .01f));
    const float verticalLimit = std::max(0.0f, std::min(.35f, (.90f - halfHeight - .01f) / viewCosine));
    position_.x = std::clamp(position_.x, -horizontalLimit, horizontalLimit);
    position_.y = std::clamp(position_.y, -verticalLimit, verticalLimit);
}

void ComputerScreen::drawCube(const glm::mat4& viewProjection, const glm::mat4& model) const {
    const GlState state;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    renderer_.begin(viewProjection);
    renderer_.drawMovableMesh(0, 36, model, glm::vec3{1.0f});
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    renderer_.drawMovableMesh(0, 36, model, {.12f,.24f,.29f});
}

void ComputerScreen::renderTexture() const {
    if (!dirty_) return;
    const GlState state;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, textureWidth, textureHeight);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(.055f,.085f,.11f,1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (visible_) {
        auto model = glm::translate(glm::mat4{1.0f}, {position_.x, position_.y, 0.0f});
        model = glm::rotate(model, yaw_, {0.0f,1.0f,0.0f});
        model = glm::scale(model, glm::vec3{edge_});
        const auto view = glm::lookAt(glm::vec3{0.0f,1.0f,4.0f}, glm::vec3{0.0f}, glm::vec3{0.0f,1.0f,0.0f});
        drawCube(glm::ortho(-1.6f,1.6f,-.9f,.9f,.1f,10.0f) * view, model);
    }
    dirty_ = false;
}

void ComputerScreen::drawTexture(GLuint texture, const glm::mat4& transform, bool flip) const {
    glUseProgram(program_);
    glUniformMatrix4fv(transform_, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform1i(flip_, flip);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vertexArray_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void ComputerScreen::draw(const glm::mat4& viewProjection) const {
    const GlState state;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    renderTexture();
    glm::mat4 model{1.0f};
    model[0] = {0.0f,0.0f,3.2f,0.0f};
    model[1] = {0.0f,1.8f,0.0f,0.0f};
    model[2] = {-1.0f,0.0f,0.0f,0.0f};
    model[3] = {center,1.0f};
    drawTexture(color_, viewProjection * model, false);
}

void ComputerScreen::drawGuide(bool active, bool holding) const {
    if (!active && !holding) return;
    const GlState state;
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) return;
    const float width = std::min(static_cast<float>(guideWidth), viewport[2] * .94f);
    const float height = width * guideHeight / guideWidth;
    auto model = glm::translate(glm::mat4{1.0f}, {0.0f,-1.0f+(height+24.0f)/viewport[3],0.0f});
    model = glm::scale(model, {width*2.0f/viewport[2],height*2.0f/viewport[3],1.0f});
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    drawTexture(active ? guide_ : holdingGuide_, model, true);
}
