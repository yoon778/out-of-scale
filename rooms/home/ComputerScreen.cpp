#include "ComputerScreen.h"

#include <windows.h>
#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int textureWidth = 1280;
constexpr int textureHeight = 720;
struct ScreenRect {
    int left, top, right, bottom;
    bool contains(float x, float y) const {
        return x >= left && x <= right && y >= top && y <= bottom;
    }
};
constexpr ScreenRect closeButton{1184, 0, 1280, 64};
constexpr ScreenRect keyFileIcon{76, 110, 270, 335};
constexpr int lessonCount = 5;
constexpr int lessonTop = 150;
constexpr int lessonStep = 76;
constexpr int lessonHeight = 64;
struct CanvasViewport { int x, y, width, height; };
constexpr CanvasViewport canvas{435, 140, 720, 540};
enum class ScreenPage { Ide, Desktop, KeyDocument, EmptyDocument };
class GlState {
public:
    GlState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer_);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer_);
        glGetIntegerv(GL_VIEWPORT, viewport_);
        glGetIntegerv(GL_SCISSOR_BOX, scissorBox_);
        glGetFloatv(GL_LINE_WIDTH, &lineWidth_);
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
        blend_ = glIsEnabled(GL_BLEND);
        cull_ = glIsEnabled(GL_CULL_FACE);
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
        restore(GL_BLEND, blend_);
        restore(GL_CULL_FACE, cull_);
        glScissor(scissorBox_[0],scissorBox_[1],scissorBox_[2],scissorBox_[3]);
        glLineWidth(lineWidth_);
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
    GLint scissorBox_[4]{};
    GLfloat lineWidth_ = 1;
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
    GLboolean blend_ = GL_FALSE;
    GLboolean cull_ = GL_FALSE;
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

GLuint screenTexture(ScreenPage page, int selected = 0) {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return 0;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = textureWidth;
    info.bmiHeader.biHeight = -textureHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) { DeleteDC(dc); return 0; }
    const auto oldBitmap = SelectObject(dc, bitmap);
    const auto fill = [&](RECT area, COLORREF color) {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &area, brush);
        DeleteObject(brush);
    };
    HFONT font = CreateFontW(-25, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
    const auto oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    const auto text = [&](const wchar_t* value, RECT rect, COLORREF color, bool centered = false) {
        SetTextColor(dc, color);
        DrawTextW(dc, value, -1, &rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
                  (centered ? DT_CENTER : DT_LEFT));
    };
    if (page == ScreenPage::Desktop) {
        fill({0, 0, textureWidth, textureHeight}, RGB(30, 53, 70));
        fill({0, 674, textureWidth, textureHeight}, RGB(26, 31, 39));
        fill({110, 132, 224, 264}, RGB(230, 231, 226));
        fill({180, 132, 224, 176}, RGB(188, 194, 193));
        fill({130, 188, 156, 216}, RGB(81, 139, 154));
        fill({156, 197, 204, 207}, RGB(171, 132, 67));
        fill({184, 205, 194, 220}, RGB(171, 132, 67));
        text(L"열쇠", {76, 280, 270, 330}, RGB(240, 242, 244), true);
    } else {
        const bool ide = page == ScreenPage::Ide;
        fill({0, 0, textureWidth, textureHeight}, RGB(30, 30, 30));
        fill({0, 0, textureWidth, 64}, RGB(55, 42, 72));
        text(ide ? L"Out of Scale - Microsoft Visual Studio" : L"열쇠",
             {26, 0, 1160, 64}, RGB(238, 237, 242));
        fill({closeButton.left, closeButton.top, closeButton.right, closeButton.bottom}, RGB(69, 49, 76));
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(242, 238, 243));
        const auto oldPen = SelectObject(dc, pen);
        MoveToEx(dc, 1218, 20, nullptr); LineTo(dc, 1246, 44);
        MoveToEx(dc, 1246, 20, nullptr); LineTo(dc, 1218, 44);
        SelectObject(dc, oldPen); DeleteObject(pen);
        if (ide) {
            constexpr const wchar_t* files[lessonCount]{L"1_7.cpp", L"1_12.cpp", L"1_14.cpp", L"1_18.cpp", L"1_26.cpp"};
            fill({0, 64, 320, 720}, RGB(37, 37, 38));
            text(L"솔루션 탐색기", {24, 72, 308, 128}, RGB(205, 205, 210));
            for (int i = 0; i < lessonCount; ++i) {
                const int top = lessonTop + i * lessonStep;
                fill({20, top, 300, top + lessonHeight},
                     i == selected ? RGB(55, 62, 76) : RGB(42, 42, 44));
                text(files[i], {38, top, 292, top + lessonHeight}, RGB(222, 224, 228));
            }
            fill({336, 78, 570, 125}, RGB(45, 45, 48));
            text(files[selected], {355, 78, 562, 125}, RGB(235, 235, 240));
            fill({canvas.x - 2, canvas.y - 2, canvas.x + canvas.width + 2,
                  canvas.y + canvas.height + 2}, RGB(12, 13, 15));
        } else if (page == ScreenPage::KeyDocument) {
            // Same head, shaft and teeth as the complete key drawn in the room.
            fill({380, 285, 560, 465}, RGB(87, 189, 217));
            fill({560, 345, 920, 405}, RGB(224, 168, 74));
            fill({760, 405, 805, 465}, RGB(224, 168, 74));
            fill({870, 405, 915, 465}, RGB(224, 168, 74));
        }
    }
    GdiFlush();
    GLuint result = 0;
    glGenTextures(1, &result);
    glBindTexture(GL_TEXTURE_2D, result);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, textureWidth, textureHeight, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    SelectObject(dc, oldFont); DeleteObject(font);
    SelectObject(dc, oldBitmap); DeleteObject(bitmap); DeleteDC(dc);
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

const std::array<MeshVertex, 72>& computerMeshes() {
    static const auto vertices = [] {
        std::array<MeshVertex, 72> result{};
        constexpr float light[6]{.62f, .95f, .72f, .82f, 1.0f, .55f};
        for (std::size_t index = 0; index < 36; ++index) {
            result[index] = cubeMesh()[index];
            result[36 + index] = {cubeMesh()[index].position,
                                  glm::vec3{.88f, .66f, .29f} * light[index / 6]};
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
    glDeleteTextures(static_cast<GLsizei>(workspaceLabels_.size()),workspaceLabels_.data());
    const GLuint labels[]{desktopLabel_, keyDocumentLabel_, emptyDocumentLabel_};
    glDeleteTextures(3, labels);
}

bool ComputerScreen::initialize() {
    const GlState state;
    glActiveTexture(GL_TEXTURE0);
    if (!renderer_.initialize({}, computerMeshes()) || !labs_.initialize()) return false;
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
    for (int i = 0; i < lessonCount; ++i) {
        workspaceLabels_[i] = screenTexture(ScreenPage::Ide, i);
        if (!workspaceLabels_[i]) return false;
    }
    desktopLabel_ = screenTexture(ScreenPage::Desktop);
    keyDocumentLabel_ = screenTexture(ScreenPage::KeyDocument);
    emptyDocumentLabel_ = screenTexture(ScreenPage::EmptyDocument);
    labs_.select(0);
    return complete && desktopLabel_ && keyDocumentLabel_ && emptyDocumentLabel_;
}

void ComputerScreen::keyInput(int key, int action, int mods) {
    if (view_ != View::Ide || key < GLFW_KEY_RIGHT || key > GLFW_KEY_UP) return;
    labs_.key(key, action, mods);
    dirty_ = true;
}
void ComputerScreen::selectLesson(int lesson) {
    if (lesson < 0 || lesson >= lessonCount) return;
    labs_.clearInput();
    labs_.select(lesson);
    dirty_ = true;
}
int ComputerScreen::selectedLesson() const { return labs_.selected(); }
void ComputerScreen::clearInput() { labs_.clearInput(); dirty_=true; }
void ComputerScreen::setView(View view) {
    labs_.clearInput();
    view_ = view;
    dirty_ = true;
}

ComputerScreen::Action ComputerScreen::click(float u, float v) {
    if (!std::isfinite(u) || !std::isfinite(v) || u < 0 || u > 1 || v < 0 || v > 1)
        return Action::None;
    const float x = u * textureWidth;
    const float y = v * textureHeight;
    if (view_ != View::Desktop && closeButton.contains(x, y)) {
        setView(View::Desktop);
        return Action::None;
    }
    if (view_ == View::Ide) {
        for (int i = 0; i < lessonCount; ++i) {
            const int top = lessonTop + i * lessonStep;
            if (ScreenRect{20, top, 300, top + lessonHeight}.contains(x, y)) {
                selectLesson(i);
                break;
            }
        }
    } else if (view_ == View::Desktop && keyFileIcon.contains(x, y)) {
        setView(View::KeyDocument);
        if (!keyTaken_) return Action::TakeCompleteKey;
    } else if (view_ == View::KeyDocument && y > closeButton.bottom && !keyTaken_) {
        return Action::TakeCompleteKey;
    }
    return Action::None;
}

std::optional<glm::vec2> ComputerScreen::rayToUv(const glm::vec3& eye,
    const glm::vec3& direction, float maxDistance) {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(eye[axis]) || !std::isfinite(direction[axis])) return std::nullopt;
    }
    if (!std::isfinite(maxDistance) || maxDistance <= 0 || eye.x >= center.x || direction.x <= 0)
        return std::nullopt;
    const float length = glm::length(direction);
    if (!std::isfinite(length) || length < 0.000001f) return std::nullopt;
    const glm::vec3 ray = direction / length;
    if (ray.x < 0.000001f) return std::nullopt;
    const float distance = (center.x - eye.x) / ray.x;
    if (distance > maxDistance) return std::nullopt;
    const glm::vec3 hit = eye + ray * distance;
    const glm::vec2 uv{(hit.z - center.z + 1.6f) / 3.2f,
                       (center.y + 0.9f - hit.y) / 1.8f};
    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1) return std::nullopt;
    return uv;
}

void ComputerScreen::update(float elapsed) {
    if (view_ == View::Ide && labs_.update(elapsed)) dirty_ = true;
}
void ComputerScreen::consumeKey() { keyTaken_ = true; dirty_ = true; }
bool ComputerScreen::keyTaken() const { return keyTaken_; }

void ComputerScreen::drawCube(const glm::mat4& viewProjection, const glm::mat4& model) const {
    const GlState state;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    renderer_.begin(viewProjection);
    renderer_.drawMovableMesh(0, 36, model, glm::vec3{1.0f});
}

void ComputerScreen::drawKeyPiece(const glm::mat4& viewProjection, const glm::mat4& model) const {
    const GlState state;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    renderer_.begin(viewProjection);
    const auto part = [&](const glm::vec3& partCenter, const glm::vec3& size) {
        const auto transform = glm::scale(glm::translate(model, partCenter), size);
        renderer_.drawMovableMesh(36, 36, transform, glm::vec3{1.0f});
    };
    part({0, .1f, 0}, {1.3f, .2f, .12f});
    part({.25f, -.1f, 0}, {.13f, .2f, .12f});
    part({.55f, -.1f, 0}, {.13f, .2f, .12f});
}

void ComputerScreen::drawCompleteKey(const glm::mat4& viewProjection, const glm::mat4& model) const {
    const GlState state;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    renderer_.begin(viewProjection);
    const auto part = [&](GLint first, const glm::vec3& partCenter, const glm::vec3& size) {
        renderer_.drawMovableMesh(first, 36, glm::scale(glm::translate(model, partCenter), size),
                                 glm::vec3{1.0f});
    };
    part(0, {-.70f, 0, 0}, {.60f, .60f, .24f});
    part(36, {.25f, .10f, 0}, {1.30f, .20f, .12f});
    part(36, {.50f, -.10f, 0}, {.13f, .20f, .12f});
    part(36, {.80f, -.10f, 0}, {.13f, .20f, .12f});
}

void ComputerScreen::renderTexture() const {
    if (!dirty_) return;
    const GlState state;
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);
    glViewport(0,0,textureWidth,textureHeight);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); glLineWidth(1);
    glDepthMask(GL_TRUE); glDepthFunc(GL_LESS); glClearDepth(1);
    glClearColor(.03f,.04f,.05f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const GLuint background = view_ == View::Ide ? workspaceLabels_[labs_.selected()]
        : view_ == View::Desktop ? desktopLabel_
        : keyTaken_ ? emptyDocumentLabel_ : keyDocumentLabel_;
    drawTexture(background, glm::scale(glm::mat4{1}, {2, 2, 1}), true);
    if (view_ == View::Ide) {
        glViewport(canvas.x, textureHeight - canvas.y - canvas.height, canvas.width, canvas.height);
        glEnable(GL_SCISSOR_TEST);
        glScissor(canvas.x, textureHeight - canvas.y - canvas.height, canvas.width, canvas.height);
        labs_.draw();
    }
    dirty_=false;
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
