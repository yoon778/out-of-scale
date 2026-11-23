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
constexpr int guideWidth = 860;
constexpr int guideHeight = 60;

struct WorkspaceViewport { int x, y, width, height; };
WorkspaceViewport workspaceCanvas(int lesson) {
    return lesson == 5 ? WorkspaceViewport{350,135,600,450}
                       : WorkspaceViewport{435,90,720,540};
}

WorkspaceViewport workspaceViewport(int width, int height) {
    const float scale = std::min(width / 1280.0f, height / 720.0f);
    const int w = std::max(1, static_cast<int>(1280 * scale));
    const int h = std::max(1, static_cast<int>(720 * scale));
    return {(width-w)/2,(height-h)/2,w,h};
}

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
    HFONT font = CreateFontW(-20, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE measured{};
    GetTextExtentPoint32W(dc,text,lstrlenW(text),&measured);
    if (measured.cx > guideWidth-16) {
        SelectObject(dc,oldFont);
        DeleteObject(font);
        const int height=std::max(15,20*(guideWidth-16)/static_cast<int>(measured.cx));
        font=CreateFontW(-height,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,HANGEUL_CHARSET,
                         OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        SelectObject(dc,font);
    }
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

GLuint workspaceTexture(int selected) {
    HDC dc=CreateCompatibleDC(nullptr);
    if (!dc) return 0;
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=textureWidth; info.bmiHeader.biHeight=-textureHeight;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if (!bitmap) { DeleteDC(dc); return 0; }
    const auto oldBitmap=SelectObject(dc,bitmap);
    const auto fill=[&](RECT area,COLORREF color) { HBRUSH brush=CreateSolidBrush(color); FillRect(dc,&area,brush); DeleteObject(brush); };
    fill({0,0,1280,720},RGB(16,24,31));
    fill({0,0,320,720},RGB(24,35,45));
    HFONT font=CreateFontW(-23,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,HANGEUL_CHARSET,
                           OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
    const auto oldFont=SelectObject(dc,font); SetBkMode(dc,TRANSPARENT);
    const auto text=[&](const wchar_t* value,RECT rect,COLORREF color) { SetTextColor(dc,color); DrawTextW(dc,value,-1,&rect,DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX); };
    text(L"컴퓨터그래픽스 실습",{26,22,310,70},RGB(229,239,244));
    constexpr const wchar_t* names[6]{L"1_7.cpp · 도형",L"1_12.cpp · 쌓기",L"1_14.cpp · 3D 변환",L"1_18.cpp · 행성",L"1_26.cpp · 조명",L"열쇠 제작"};
    constexpr const wchar_t* captions[6]{L"실습 1_7  ·  점 / 선 / 삼각형 / 사각형",L"실습 1_12  ·  움직이는 사각형 맞추기",L"실습 1_14  ·  이동 / 회전 / 깊이",L"실습 1_18  ·  부모와 자식의 변환",L"실습 1_26  ·  Phong 조명과 광원 공전",L"열쇠 제작"};
    for(int i=0;i<6;++i) {
        fill({20,100+i*76,300,164+i*76},i==selected?RGB(48,94,111):RGB(32,46,57));
        text(names[i],{36,100+i*76,294,164+i*76},RGB(236,242,245));
    }
    text(captions[selected],{355,24,1260,76},RGB(223,235,241));
    if (selected == 5) {
        fill({980,135,1260,455},RGB(24,35,45));
        text(L"완성 모양",{1000,150,1240,194},RGB(223,235,241));
        fill({995,205,1245,405},RGB(14,22,29));
        // Same proportions, orientation and colors as the assembled 2D key.
        // X:Y scale matches the 4:3 workspace; this picture has no hit target.
        const auto part = [&](float x,float y,float width,float height,COLORREF color) {
            const auto px = [](float value) { return static_cast<LONG>(std::lround(1035+180*value)); };
            const auto py = [](float value) { return static_cast<LONG>(std::lround(305-135*value)); };
            fill({px(x-width*.5f),py(y+height*.5f),px(x+width*.5f),py(y-height*.5f)},color);
        };
        part(0,0,.40f,.40f,RGB(87,189,217));
        part(.61f,.07f,.86f,.14f,RGB(224,168,74));
        part(.77f,-.07f,.09f,.14f,RGB(224,168,74));
        part(.99f,-.07f,.09f,.14f,RGB(224,168,74));
    }
    text(L"목록을 마우스로 선택",{30,598,310,642},RGB(155,180,194));
    text(L"Tab  방으로 돌아가기",{30,644,310,690},RGB(195,216,226));
    GdiFlush();
    GLuint texture=0; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,textureWidth,textureHeight,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);
    return texture;
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
    glDeleteTextures(static_cast<GLsizei>(helpLabels_.size()),helpLabels_.data());
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
    constexpr const wchar_t* help[6]{
        L"P 점 · E 선 · T 삼각형 · R 사각형 | 클릭 선택 · WASD/IJKL 이동 · 1~4 전체 · C 비우기",
        L"두 사각형이 점선 안에 들어오면 Enter · R 다시 시작",
        L"C 육면체 / P 사각뿔 · X/Y 회전(Shift 반대) · 방향키 이동 · W/Shift+W · H 깊이 · S 초기화",
        L"WASD 이동 · +/- 깊이 · P/Shift+P 투영 · M/Shift+M 와이어 · 1/2/3 애니메이션",
        L"N 도형 · M 조명 · Y 회전 · R/Shift+R 광원 공전 · Z/Shift+Z 광원 거리",
        L"도형 클릭·드래그 · Q/R 회전 · 휠 크기 · E 확인"};
    for (int i=0;i<6;++i) {
        workspaceLabels_[i]=workspaceTexture(i);
        helpLabels_[i]=guideTexture(help[i]);
        if (!workspaceLabels_[i] || !helpLabels_[i]) return false;
    }
    return complete;
}

void ComputerScreen::keyInput(int key,int action,int mods) {
    labs_.key(key,action,mods); dirty_=true;
}
void ComputerScreen::selectLesson(int lesson) {
    labs_.select(lesson); dirty_=true;
}
int ComputerScreen::selectedLesson() const { return labs_.selected(); }
void ComputerScreen::clearInput() { labs_.clearInput(); dirty_=true; }
void ComputerScreen::pointer(float u,float v) {
    int width=0,height=0;
    if (auto* window=glfwGetCurrentContext()) glfwGetFramebufferSize(window,&width,&height);
    if (width<=0 || height<=0) return;
    const auto area=workspaceViewport(width,height);
    const auto canvas=workspaceCanvas(labs_.selected());
    pointer_={(u*width-area.x)*textureWidth/area.width,(v*height-area.y)*textureHeight/area.height};
    canvasPointer_=pointer_.x>=canvas.x && pointer_.x<=canvas.x+canvas.width &&
                   pointer_.y>=canvas.y && pointer_.y<=canvas.y+canvas.height;
    labs_.pointer({(pointer_.x-canvas.x)*2/canvas.width-1,1-(pointer_.y-canvas.y)*2/canvas.height},canvasPointer_);
    dirty_=true;
}
void ComputerScreen::mouseButton(int button,int action,int) {
    if (button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_PRESS && pointer_.x>=20 && pointer_.x<=300) {
        for(int i=0;i<6;++i) if(pointer_.y>=100+i*76 && pointer_.y<=164+i*76) {
            selectLesson(i); return;
        }
    }
    labs_.mouse(button,action); dirty_=true;
}
void ComputerScreen::scroll(double amount) { labs_.scroll(amount); dirty_=true; }
void ComputerScreen::update(float elapsed) { if (labs_.update(elapsed)) dirty_=true; }
ComputerScreen::Action ComputerScreen::confirm() {
    if (labs_.selected()!=5 || labs_.keyTaken()) return Action::None;
    if (!labs_.keyReady()) return Action::None;
    return Action::TakeCompleteKey;
}
void ComputerScreen::consumeKey() { labs_.consumeKey(); dirty_=true; }
bool ComputerScreen::keyTaken() const { return labs_.keyTaken(); }

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
    drawTexture(workspaceLabels_[labs_.selected()],glm::scale(glm::mat4{1},{2,2,1}),true);
    const auto canvas=workspaceCanvas(labs_.selected());
    glViewport(canvas.x,textureHeight-canvas.y-canvas.height,canvas.width,canvas.height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(canvas.x,textureHeight-canvas.y-canvas.height,canvas.width,canvas.height);
    labs_.draw();
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST);
    glViewport(0,0,textureWidth,textureHeight); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    const GLuint help=helpLabels_[labs_.selected()];
    // Footer uses the same top-left layout as pointer hit-testing.
    auto footer=glm::translate(glm::mat4{1},{2*795.0f/textureWidth-1,1-2*674.0f/textureHeight,0});
    footer=glm::scale(footer,{930.0f*2/textureWidth,60.0f*2/textureHeight,1});
    drawTexture(help,footer,true);
    dirty_=false;
}

void ComputerScreen::drawWorkspace() const {
    const GlState state;
    renderTexture();
    GLint viewport[4]{}; glGetIntegerv(GL_VIEWPORT,viewport);
    const auto area=workspaceViewport(viewport[2],viewport[3]);
    glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    // Clear the full destination so non-16:9 windows have quiet letterbox bars.
    glClearColor(.01f,.015f,.02f,1); glClear(GL_COLOR_BUFFER_BIT);
    glViewport(viewport[0]+area.x,viewport[1]+area.y,area.width,area.height);
    drawTexture(color_,glm::scale(glm::mat4{1},{2,2,1}),false);
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
