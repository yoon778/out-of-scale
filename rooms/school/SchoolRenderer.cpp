#include "SchoolRenderer.h"
#include <windows.h>
#include <gdiplus.h>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace school {
namespace {
constexpr int reflectionSize = 1024;

std::array<glm::vec4, 7> viewPlanes(const glm::mat4& vp, glm::vec4 clipPlane = {0, 0, 0, 1}) {
    // GLM matrices are column-major. Clip space keeps -w <= x, y, z <= w.
    const auto rows = glm::transpose(vp);
    return {rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1], rows[3] - rows[1],
            rows[3] + rows[2], rows[3] - rows[2], clipPlane};
}

bool intersectsView(const std::array<glm::vec4, 7>& planes, glm::vec3 center, glm::vec3 halfExtents) {
    // Test the farthest corner from each plane, with a small margin at the screen edges.
    halfExtents += glm::vec3(.002f);
    for (glm::vec4 plane : planes) {
        const auto normal = glm::vec3(plane);
        if (glm::dot(normal, center) + plane.w + glm::dot(glm::abs(normal), halfExtents) < 0)
            return false;
    }
    return true;
}
const char* vertexSource = R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 color;
layout(location=3) in vec2 uv;
uniform mat4 viewProjection;
uniform mat4 reflectedViewProjection;
out vec3 world, surfaceNormal, tint;
out vec2 texCoord;
out vec4 reflected;
void main() {
    world=position; surfaceNormal=normal; tint=color; texCoord=uv;
    reflected=reflectedViewProjection*vec4(position,1);
    gl_Position=viewProjection*vec4(position,1);
})";
const char* fragmentSource = R"(#version 330 core
in vec3 world, surfaceNormal, tint;
in vec2 texCoord;
in vec4 reflected;
uniform sampler2D image;
uniform int mode;
uniform vec3 eye;
uniform vec4 clipPlane;
uniform vec3 projectorOrigin;
uniform mat3 projectorInverse;
uniform vec4 slideBounds;
uniform float slidePlane;
uniform sampler2D artwork;
uniform mat3 artworkInverse;
uniform vec4 artworkFrame;
uniform float opacity;
out vec4 pixel;
void main() {
    if (dot(vec4(world,1),clipPlane)<0) discard;
    if(mode==1) {
        float alpha=texture(image,texCoord).r;
        if(alpha<0.03) discard;
        pixel=vec4(tint,alpha); return;
    }
    if(mode==2) {
        vec2 uv=reflected.xy/reflected.w*0.5+0.5;
        vec3 reflection=texture(image,uv).rgb;
        float edge=min(min(texCoord.x,1-texCoord.x),min(texCoord.y,1-texCoord.y));
        pixel=vec4(mix(vec3(0.55,0.64,0.61),reflection,smoothstep(0,0.018,edge)*0.985),1);
        return;
    }
    if(mode==3) { pixel=vec4(tint,1); return; }
    if(mode==4 || mode==5 || mode==6) {
        vec3 ray=projectorInverse*(world-projectorOrigin);
        if(ray.z>=-0.001) discard;
        vec2 source=(projectorOrigin+ray*((slidePlane-projectorOrigin.z)/ray.z)).xy;
        vec2 uv=(source-slideBounds.xy)/(slideBounds.zw-slideBounds.xy);
        vec2 margin=min(source-slideBounds.xy,slideBounds.zw-source);
        float feather=smoothstep(0.0,0.18,margin.x)*smoothstep(0.0,0.18,margin.y);
        if(feather<0.001) discard;
        float grain=0.97+0.03*sin(source.x*137.0+sin(source.y*79.0));
        vec3 artRay=artworkInverse*(world-projectorOrigin);
        vec2 artPoint=(projectorOrigin+artRay*((-4.50-projectorOrigin.z)/min(artRay.z,-0.001))).xy;
        vec2 artSize=artworkFrame.zw-artworkFrame.xy;
        vec2 artUV=(artPoint-artworkFrame.xy)/artSize;
        vec2 artMargin=min(artUV,vec2(1)-artUV);
        float artCoverage=smoothstep(0.0,0.025,artMargin.x)*smoothstep(0.0,0.025,artMargin.y);
        artCoverage*=step(artRay.z,-0.001);
        if(mode==6) {
            float alpha=feather*grain*0.23*opacity*mix(1.0,artCoverage,step(world.z,-4.3));
            pixel=vec4(tint,alpha); return;
        }
        if(mode==4) {
            // A real art-class reference is the main image; game contours are construction marks.
            float falloff=1.0-0.35*dot(uv-0.5,uv-0.5);
            float spill=texture(image,uv).r*feather*falloff*grain*0.18;
            spill*=mix(1.0,artCoverage,step(world.z,-4.3));
            // Grazing light leaves the wood grain visible instead of turning the floor into paper.
            float incidence=abs(dot(normalize(surfaceNormal),normalize(projectorOrigin-world)));
            float artAlpha=artCoverage*feather*0.92*clamp(incidence*1.5,0.12,1.0);
            float alpha=artAlpha+spill*(1.0-artAlpha);
            // Keep the complete panoramic arrangement inside the green panels, with paper bleed
            // covering the board edges. The frame has overscan for the three different panel depths.
            vec2 photoSize=vec2(textureSize(artwork,0));
            vec2 photoUV=(artUV-0.5)*vec2((artSize.x/artSize.y)/(photoSize.x/photoSize.y),1.0)*1.22+0.5;
            vec2 photoMargin=min(photoUV,vec2(1)-photoUV);
            float photoEdge=smoothstep(0.0,0.02,min(photoMargin.x,photoMargin.y));
            vec3 paper=texture(artwork,vec2(0.99,0.99)).rgb;
            vec3 study=mix(paper,texture(artwork,clamp(photoUV,vec2(0),vec2(1))).rgb,photoEdge)*vec3(1.0,0.99,0.95);
            pixel=vec4(mix(tint,study,artAlpha/max(alpha,0.001)),alpha*opacity); return;
        }
        float across=abs(texCoord.y-0.5)*2.0;
        float core=1.0-smoothstep(0.20,0.70,across);
        float halo=1.0-smoothstep(0.40,1.0,across);
        float alpha=(core*0.60+halo*0.18)*feather*grain*mix(1.0,0.16,artCoverage);
        // The chalkboard shows the still life, with quiet construction lines inside the picture.
        alpha*=mix(1.0,artCoverage,step(world.z,-4.3));
        pixel=vec4(tint,alpha*opacity); return;
    }
    vec3 n=normalize(surfaceNormal);
    float diffuse=max(dot(n,normalize(vec3(-0.8,1,0.35))),0);
    vec3 c=tint*(0.70+0.30*diffuse);
    // Restrained procedural grain on wooden materials.
    if(tint.r>tint.g*1.22 && tint.g>tint.b*1.17) {
        float grain=sin(world.x*73+sin(world.z*17)*0.6)*sin(world.z*4.2);
        c*=0.98+grain*0.025;
    }
    float haze=clamp(length(world-eye)/40,0,0.10);
    pixel=vec4(mix(c,vec3(0.85,0.82,0.70),haze),opacity);
})";
GLuint compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint good = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &good);
    if (!good) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
GLuint loadArtwork() {
    wchar_t module[32768]{};
    if (!GetModuleFileNameW(nullptr, module, 32768))
        return 0;
    auto path = std::filesystem::path(module).parent_path() / "assets" / "art-class-still-life-v2.png";
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok)
        return 0;
    GLuint texture = 0;
    {
        Gdiplus::Bitmap bitmap(path.c_str());
        if (bitmap.GetLastStatus() == Gdiplus::Ok) {
            int width = static_cast<int>(bitmap.GetWidth()), height = static_cast<int>(bitmap.GetHeight());
            Gdiplus::Rect rect(0, 0, width, height);
            Gdiplus::BitmapData data{};
            if (width > 0 && height > 0 &&
                bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) ==
                    Gdiplus::Ok) {
                std::vector<unsigned char> rgba(static_cast<std::size_t>(width) * height * 4);
                for (int y = 0; y < height; ++y) {
                    auto row = static_cast<unsigned char*>(data.Scan0) +
                               static_cast<std::ptrdiff_t>(y) * data.Stride;
                    auto out = rgba.data() + static_cast<std::size_t>(height - 1 - y) * width * 4;
                    for (int x = 0; x < width; ++x) {
                        out[x * 4] = row[x * 4 + 2];
                        out[x * 4 + 1] = row[x * 4 + 1];
                        out[x * 4 + 2] = row[x * 4];
                        out[x * 4 + 3] = row[x * 4 + 3];
                    }
                }
                bitmap.UnlockBits(&data);
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             rgba.data());
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        }
    }
    Gdiplus::GdiplusShutdown(token);
    if (!texture)
        std::cerr << "Could not load art-class still life: " << path << '\n';
    return texture;
}
} // namespace

Renderer::~Renderer() {
    if (artworkTexture_)
        glDeleteTextures(1, &artworkTexture_);
    if (projectionMask_)
        glDeleteTextures(1, &projectionMask_);
    for (auto& item : textTextures_)
        glDeleteTextures(1, &item.second.id);
    if (reflectionDepth_)
        glDeleteRenderbuffers(1, &reflectionDepth_);
    if (reflectionTexture_)
        glDeleteTextures(1, &reflectionTexture_);
    if (reflectionBuffer_)
        glDeleteFramebuffers(1, &reflectionBuffer_);
    if (vertexBuffer_)
        glDeleteBuffers(1, &vertexBuffer_);
    if (vertexArray_)
        glDeleteVertexArrays(1, &vertexArray_);
    if (program_)
        glDeleteProgram(program_);
}
bool Renderer::initialize() {
    GLuint v = compile(GL_VERTEX_SHADER, vertexSource), f = compile(GL_FRAGMENT_SHADER, fragmentSource);
    if (!v || !f) {
        if (v)
            glDeleteShader(v);
        if (f)
            glDeleteShader(f);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, v);
    glAttachShader(program_, f);
    glLinkProgram(program_);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint good = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &good);
    if (!good) {
        char log[2048]{};
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        std::cerr << log << '\n';
        return false;
    }
    glGenVertexArrays(1, &vertexArray_);
    glBindVertexArray(vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    const int counts[] = {3, 3, 3, 2};
    const size_t offsets[] = {offsetof(Vertex, position), offsetof(Vertex, normal), offsetof(Vertex, color),
                              offsetof(Vertex, uv)};
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, counts[i], GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsets[i]));
    }
    glGenFramebuffers(1, &reflectionBuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionBuffer_);
    glGenTextures(1, &reflectionTexture_);
    glBindTexture(GL_TEXTURE_2D, reflectionTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, reflectionSize, reflectionSize, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflectionTexture_, 0);
    glGenRenderbuffers(1, &reflectionDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, reflectionDepth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, reflectionSize, reflectionSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflectionDepth_);
    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    artworkTexture_ = loadArtwork();
    return complete && artworkTexture_ != 0;
}
void Renderer::begin(const glm::mat4& vp, glm::vec3 eye, glm::vec4 plane) {
    viewPlanes_ = viewPlanes(vp, plane);
    glUseProgram(program_);
    glBindVertexArray(vertexArray_);
    glUniformMatrix4fv(glGetUniformLocation(program_, "viewProjection"), 1, GL_FALSE, glm::value_ptr(vp));
    glUniform3fv(glGetUniformLocation(program_, "eye"), 1, glm::value_ptr(eye));
    glUniform4fv(glGetUniformLocation(program_, "clipPlane"), 1, glm::value_ptr(plane));
    glUniform1i(glGetUniformLocation(program_, "mode"), 0);
    glUniform1i(glGetUniformLocation(program_, "image"), 0);
    glUniform1f(glGetUniformLocation(program_, "opacity"), 1);
}
bool Renderer::visible(glm::vec3 center, glm::vec3 halfExtents) const {
    return intersectsView(viewPlanes_, center, halfExtents);
}
bool Renderer::visible(const glm::mat4& viewProjection, glm::vec3 center, glm::vec3 halfExtents) {
    return intersectsView(viewPlanes(viewProjection), center, halfExtents);
}
void Renderer::quad(glm::vec3 c, glm::vec3 r, glm::vec3 u, glm::vec3 color) {
    if (visible(c, glm::abs(r) + glm::abs(u)))
        appendQuad(c, r, u, color);
}
void Renderer::appendQuad(glm::vec3 c, glm::vec3 r, glm::vec3 u, glm::vec3 color) {
    glm::vec3 n = glm::normalize(glm::cross(r, u));
    const glm::vec3 p[] = {c - r - u, c + r - u, c + r + u, c - r + u};
    const glm::vec2 uv[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i : {0, 1, 2, 0, 2, 3})
        vertices_.push_back({p[i], n, color, uv[i]});
}
void Renderer::triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color) {
    const auto low = glm::min(a, glm::min(b, c)), high = glm::max(a, glm::max(b, c));
    if (!visible((low + high) * .5f, (high - low) * .5f))
        return;
    auto normal = glm::cross(b - a, c - a);
    if (glm::length(normal) < 0.000001f)
        return;
    normal = glm::normalize(normal);
    for (auto point : {a, b, c})
        vertices_.push_back({point, normal, color, {0, 0}});
}
void Renderer::beginProjection(glm::vec3 origin, const glm::mat3& inverseRotation, glm::vec4 bounds,
                               float planeZ, const std::vector<float>& mask, int resolution,
                               const glm::mat3& artworkInverse, glm::vec4 artworkFrame, float brightness) {
    flush();
    glActiveTexture(GL_TEXTURE0);
    if (!projectionMask_) {
        glGenTextures(1, &projectionMask_);
        glBindTexture(GL_TEXTURE_2D, projectionMask_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, resolution, resolution, 0, GL_RED, GL_FLOAT, mask.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, projectionMask_);
    }
    glUniform3fv(glGetUniformLocation(program_, "projectorOrigin"), 1, glm::value_ptr(origin));
    glUniformMatrix3fv(glGetUniformLocation(program_, "projectorInverse"), 1, GL_FALSE,
                       glm::value_ptr(inverseRotation));
    glUniform4fv(glGetUniformLocation(program_, "slideBounds"), 1, glm::value_ptr(bounds));
    glUniform1f(glGetUniformLocation(program_, "slidePlane"), planeZ);
    glUniformMatrix3fv(glGetUniformLocation(program_, "artworkInverse"), 1, GL_FALSE,
                       glm::value_ptr(artworkInverse));
    glUniform1i(glGetUniformLocation(program_, "artwork"), 1);
    glUniform4fv(glGetUniformLocation(program_, "artworkFrame"), 1, glm::value_ptr(artworkFrame));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, artworkTexture_);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(program_, "mode"), 4);
    glUniform1f(glGetUniformLocation(program_, "opacity"), brightness);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
}
void Renderer::projectionStrokes(float alpha) {
    flush();
    glUniform1i(glGetUniformLocation(program_, "mode"), 5);
    glUniform1f(glGetUniformLocation(program_, "opacity"), alpha);
}
void Renderer::projectionWash(float alpha) {
    flush();
    glUniform1i(glGetUniformLocation(program_, "mode"), 6);
    glUniform1f(glGetUniformLocation(program_, "opacity"), alpha);
}
void Renderer::opacity(float alpha) {
    flush();
    glUniform1f(glGetUniformLocation(program_, "opacity"), alpha);
    if (alpha < 1) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    } else {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}
void Renderer::projectedStroke(glm::vec3 a, glm::vec3 b, glm::vec3 normal, float width, glm::vec3 color) {
    if (glm::distance(a, b) < 0.00001f)
        return;
    auto side = glm::normalize(glm::cross(normal, b - a)) * width / 2.0f;
    auto along = (b - a) / 2.0f + glm::normalize(b - a) * (width * 0.12f);
    quad((a + b) / 2.0f, along, side, color);
}
void Renderer::endProjection() {
    flush();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform1i(glGetUniformLocation(program_, "mode"), 0);
    glUniform1f(glGetUniformLocation(program_, "opacity"), 1);
}
void Renderer::box(glm::vec3 c, glm::vec3 size, glm::vec3 color, float yaw) {
    glm::vec3 r = {std::cos(yaw) * size.x / 2, 0, -std::sin(yaw) * size.x / 2};
    glm::vec3 u = {0, size.y / 2, 0}, f = {std::sin(yaw) * size.z / 2, 0, std::cos(yaw) * size.z / 2};
    if (!visible(c, glm::abs(r) + glm::abs(u) + glm::abs(f)))
        return;
    appendQuad(c + f, r, u, color);
    appendQuad(c - f, -r, u, color);
    appendQuad(c + r, -f, u, color);
    appendQuad(c - r, f, u, color);
    appendQuad(c + u, r, -f, color);
    appendQuad(c - u, r, f, color);
}
void Renderer::line(glm::vec3 a, glm::vec3 b, float width, glm::vec3 color) {
    glm::vec3 d = b - a;
    if (glm::length(d) < 0.00001f)
        return;
    glm::vec3 axis = std::abs(glm::normalize(d).y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 r = glm::normalize(glm::cross(d, axis)) * width / 2.0f;
    glm::vec3 s = glm::normalize(glm::cross(d, r)) * width / 2.0f;
    quad((a + b) / 2.0f, r, d / 2.0f, color);
    quad((a + b) / 2.0f, s, d / 2.0f, color);
}
void Renderer::flush() {
    if (vertices_.empty())
        return;
    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(Vertex), vertices_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    vertices_.clear();
}
Renderer::TextTexture& Renderer::textureFor(const std::wstring& text) {
    auto found = textTextures_.find(text);
    if (found != textTextures_.end())
        return found->second;
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font =
        CreateFontW(-48, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE extent{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &extent);
    int width = std::max(8L, extent.cx + 8), height = 72;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap || !pixels) {
        SelectObject(dc, oldFont);
        DeleteObject(font);
        DeleteDC(dc);
        throw std::runtime_error("Could not create school text texture");
    }
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    RECT rect{0, 0, width, height};
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();
    TextTexture texture;
    texture.aspect = static_cast<float>(width) / height;
    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    SelectObject(dc, oldBitmap);
    SelectObject(dc, oldFont);
    DeleteObject(bitmap);
    DeleteObject(font);
    DeleteDC(dc);
    return textTextures_.emplace(text, texture).first->second;
}
void Renderer::drawTexture(GLuint texture, int mode, glm::vec3 c, glm::vec3 r, glm::vec3 u, glm::vec3 color,
                           const glm::mat4& reflected) {
    flush();
    glUniform1i(glGetUniformLocation(program_, "mode"), mode);
    glUniformMatrix4fv(glGetUniformLocation(program_, "reflectedViewProjection"), 1, GL_FALSE,
                       glm::value_ptr(reflected));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    appendQuad(c, r, u, color);
    flush();
    glDisable(GL_BLEND);
    glUniform1i(glGetUniformLocation(program_, "mode"), 0);
}
void Renderer::label(const std::wstring& text, glm::vec3 c, glm::vec3 r, glm::vec3 u, glm::vec3 color) {
    if (!visible(c, glm::abs(r) + glm::abs(u)))
        return;
    auto& texture = textureFor(text);
    drawTexture(texture.id, 1, c, r, u, color, glm::mat4(1));
}
void Renderer::sign(const std::wstring& text, glm::vec3 c, glm::vec3 r, glm::vec3 u) {
    if (!visible(c, glm::abs(r) + glm::abs(u) + glm::vec3(.01f)))
        return;
    const auto normal = glm::normalize(glm::cross(r, u));
    quad(c, r, u, {.36f, .41f, .36f});
    quad(c + normal * .003f, r * .96f, u * .88f, {.21f, .32f, .28f});
    const auto textUp = u * .79f;
    const float textSpan = glm::length(textUp) * textureFor(text).aspect;
    const auto textRight = r * std::min(.90f, textSpan / glm::length(r));
    label(text, c + normal * .006f, textRight, textUp, {.97f, .96f, .87f});
}
void Renderer::mirror(glm::vec3 c, glm::vec3 r, glm::vec3 u, const glm::mat4& vp) {
    drawTexture(reflectionTexture_, 2, c, r, u, {1, 1, 1}, vp);
}
void Renderer::beginReflection() {
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionBuffer_);
    glViewport(0, 0, reflectionSize, reflectionSize);
    glClearColor(0.66f, 0.74f, 0.76f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void Renderer::endReflection(int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
}
void Renderer::overlay(int w, int h) {
    flush();
    glDisable(GL_DEPTH_TEST);
    begin(glm::ortho(0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, -1.0f, 1.0f), {0, 0, 0});
}
float Renderer::textWidth(const std::wstring& text, float height) {
    return textureFor(text).aspect * height;
}
void Renderer::text(const std::wstring& t, float x, float y, float h, glm::vec3 color) {
    const float w = textWidth(t, h);
    label(t, {x + w / 2, y + h / 2, 0}, {w / 2, 0, 0}, {0, -h / 2, 0}, color);
}
void Renderer::panel(float x, float y, float w, float h, glm::vec3 color) {
    flush();
    glUniform1i(glGetUniformLocation(program_, "mode"), 3);
    quad({x + w / 2, y + h / 2, 0}, {w / 2, 0, 0}, {0, h / 2, 0}, color);
    flush();
    glUniform1i(glGetUniformLocation(program_, "mode"), 0);
}
bool Renderer::saveFrame(const std::string& path, int w, int h) const {
    std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << w << ' ' << h << "\n255\n";
    for (int y = h - 1; y >= 0; --y)
        out.write(reinterpret_cast<char*>(rgb.data() + static_cast<size_t>(y) * w * 3), w * 3);
    return static_cast<bool>(out);
}
glm::vec3 Renderer::reflectPoint(glm::vec3 p, glm::vec3 c, glm::vec3 n) {
    return p - 2.0f * glm::dot(p - c, n) * n;
}
} // namespace school
