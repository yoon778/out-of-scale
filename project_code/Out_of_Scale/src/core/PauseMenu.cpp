#include "PauseMenu.h"

#include <windows.h>
#include <GL/glew.h>
#include <GL/glfw3.h>

#include <algorithm>

namespace {
constexpr int panelWidth = 420;
constexpr int panelHeight = 244;
constexpr RECT resumeButton{32, 89, 388, 143};
constexpr RECT quitButton{32, 154, 388, 208};

constexpr char vertexSource[] = R"(#version 330 core
void main() {
    const vec2 vertices[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
    gl_Position = vec4(vertices[gl_VertexID], 0, 1);
}
)";

constexpr char fragmentSource[] = R"(#version 330 core
uniform sampler2D menuTexture;
uniform vec4 panel;
uniform float frameHeight;
uniform vec4 hover;
out vec4 pixel;
void main() {
    vec2 uv = (vec2(gl_FragCoord.x, frameHeight - gl_FragCoord.y) - panel.xy) / panel.zw;
    if (any(lessThan(uv, vec2(0))) || any(greaterThanEqual(uv, vec2(1)))) {
        pixel = vec4(0.022, 0.032, 0.035, 0.65);
        return;
    }
    vec3 color = texture(menuTexture, uv).rgb;
    vec2 menuPixel = uv * vec2(textureSize(menuTexture, 0));
    if (all(greaterThanEqual(menuPixel, hover.xy)) && all(lessThan(menuPixel, hover.zw))) {
        color = mix(color, vec3(0.83, 0.89, 0.87), 0.15);
    }
    pixel = vec4(color, 1.0);
}
)";

struct Panel {
    float x;
    float y;
    float scale;
};

Panel panelFor(int width, int height) {
    const float scale = std::min(width / 1024.0f, height / 640.0f);
    return {(width - panelWidth * scale) * 0.5f, (height - panelHeight * scale) * 0.5f, scale};
}

bool contains(const RECT& rect, double x, double y) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

const RECT* buttonAtCursor(GLFWwindow* window, int width, int height, const Panel& panel) {
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0 || panel.scale <= 0.0f) {
        return nullptr;
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    x = (x * width / windowWidth - panel.x) / panel.scale;
    y = (y * height / windowHeight - panel.y) / panel.scale;
    if (contains(resumeButton, x, y)) {
        return &resumeButton;
    }
    return contains(quitButton, x, y) ? &quitButton : nullptr;
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    if (!shader) {
        return 0;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint createMenuProgram() {
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

void fill(HDC dc, RECT rect, COLORREF color) {
    const HBRUSH brush = CreateSolidBrush(color);
    if (brush) {
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }
}

bool drawLabel(HDC dc, const wchar_t* label, RECT rect, int size, int weight, COLORREF color, UINT align) {
    const HFONT font = CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
    if (!font) {
        return false;
    }
    const HGDIOBJ previousFont = SelectObject(dc, font);
    SetTextColor(dc, color);
    const bool drawn = DrawTextW(dc, label, -1, &rect, align | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) > 0;
    SelectObject(dc, previousFont);
    DeleteObject(font);
    return drawn;
}

GLuint createMenuTexture() {
    const HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        return 0;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = panelWidth;
    info.bmiHeader.biHeight = -panelHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    const HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) {
        DeleteDC(dc);
        return 0;
    }

    const HGDIOBJ previousBitmap = SelectObject(dc, bitmap);
    SetBkMode(dc, TRANSPARENT);
    fill(dc, {0, 0, panelWidth, panelHeight}, RGB(74, 91, 89));
    fill(dc, {1, 1, panelWidth - 1, panelHeight - 1}, RGB(28, 39, 40));
    fill(dc, resumeButton, RGB(56, 91, 82));
    fill(dc, quitButton, RGB(61, 72, 73));

    const bool labeled =
        drawLabel(dc, L"일시정지", {32, 23, 388, 73}, 30, FW_BOLD, RGB(247, 248, 240), DT_LEFT) &&
        drawLabel(dc, L"이어하기", {49, 89, 250, 143}, 22, FW_SEMIBOLD, RGB(250, 250, 244), DT_LEFT) &&
        drawLabel(dc, L"Esc / Enter", {255, 89, 369, 143}, 15, FW_MEDIUM, RGB(224, 239, 232), DT_RIGHT) &&
        drawLabel(dc, L"종료", {49, 154, 250, 208}, 22, FW_SEMIBOLD, RGB(250, 250, 244), DT_LEFT) &&
        drawLabel(dc, L"Q", {255, 154, 369, 208}, 15, FW_MEDIUM, RGB(225, 232, 231), DT_RIGHT);
    GdiFlush();

    GLuint texture = 0;
    if (labeled) {
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, panelWidth, panelHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, previousTexture);
    }

    SelectObject(dc, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return texture;
}

class OverlayState {
public:
    OverlayState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray_);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb_);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb_);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha_);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha_);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb_);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha_);

        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    ~OverlayState() {
        glBindTexture(GL_TEXTURE_2D, texture_);
        glActiveTexture(activeTexture_);
        glUseProgram(program_);
        glBindVertexArray(vertexArray_);
        glBlendEquationSeparate(blendEquationRgb_, blendEquationAlpha_);
        glBlendFuncSeparate(blendSrcRgb_, blendDstRgb_, blendSrcAlpha_, blendDstAlpha_);
        restore(GL_DEPTH_TEST, depth_);
        restore(GL_SCISSOR_TEST, scissor_);
        restore(GL_CULL_FACE, cull_);
        restore(GL_BLEND, blend_);
    }

private:
    static void restore(GLenum capability, GLboolean enabled) {
        if (enabled) {
            glEnable(capability);
        } else {
            glDisable(capability);
        }
    }

    GLboolean depth_ = glIsEnabled(GL_DEPTH_TEST);
    GLboolean scissor_ = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean cull_ = glIsEnabled(GL_CULL_FACE);
    GLboolean blend_ = glIsEnabled(GL_BLEND);
    GLint program_ = 0;
    GLint vertexArray_ = 0;
    GLint activeTexture_ = GL_TEXTURE0;
    GLint texture_ = 0;
    GLint blendSrcRgb_ = GL_ONE;
    GLint blendDstRgb_ = GL_ZERO;
    GLint blendSrcAlpha_ = GL_ONE;
    GLint blendDstAlpha_ = GL_ZERO;
    GLint blendEquationRgb_ = GL_FUNC_ADD;
    GLint blendEquationAlpha_ = GL_FUNC_ADD;
};
} // namespace

PauseMenu::~PauseMenu() {
    if (texture_) {
        glDeleteTextures(1, &texture_);
    }
    if (vertexArray_) {
        glDeleteVertexArrays(1, &vertexArray_);
    }
    if (program_) {
        glDeleteProgram(program_);
    }
}

bool PauseMenu::initialize() {
    program_ = createMenuProgram();
    if (!program_) {
        return false;
    }
    texture_ = createMenuTexture();
    if (!texture_) {
        return false;
    }
    glGenVertexArrays(1, &vertexArray_);
    panelLocation_ = glGetUniformLocation(program_, "panel");
    heightLocation_ = glGetUniformLocation(program_, "frameHeight");
    hoverLocation_ = glGetUniformLocation(program_, "hover");
    return vertexArray_ != 0;
}

bool PauseMenu::isPaused() const {
    return paused_;
}

void PauseMenu::setPaused(bool paused) {
    paused_ = paused;
}

bool PauseMenu::keyPressed(GLFWwindow* window, int key) {
    if (key == GLFW_KEY_ESCAPE) {
        setPaused(!paused_);
        return true;
    }
    if (!paused_) {
        return false;
    }
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
        setPaused(false);
        return true;
    }
    if (key == GLFW_KEY_Q) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return true;
    }
    return false;
}

void PauseMenu::click(GLFWwindow* window) {
    if (!paused_) {
        return;
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const RECT* button = buttonAtCursor(window, width, height, panelFor(width, height));
    if (button == &resumeButton) {
        setPaused(false);
    } else if (button == &quitButton) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void PauseMenu::draw(GLFWwindow* window, int width, int height) const {
    if (!paused_ || width <= 0 || height <= 0) {
        return;
    }
    const Panel panel = panelFor(width, height);
    const RECT* hover = buttonAtCursor(window, width, height, panel);

    const OverlayState state;
    glUseProgram(program_);
    glBindVertexArray(vertexArray_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform4f(panelLocation_, panel.x, panel.y, panelWidth * panel.scale, panelHeight * panel.scale);
    glUniform1f(heightLocation_, static_cast<float>(height));
    if (hover) {
        glUniform4f(hoverLocation_, static_cast<float>(hover->left), static_cast<float>(hover->top),
                    static_cast<float>(hover->right), static_cast<float>(hover->bottom));
    } else {
        glUniform4f(hoverLocation_, 0, 0, 0, 0);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
