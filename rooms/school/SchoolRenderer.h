#pragma once

#include <GL/glew.h>
#include <GL/glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

namespace school {
struct Vertex {
    glm::vec3 position, normal, color;
    glm::vec2 uv;
};

// Room-owned renderer; it does not change the team's shared renderer.
class Renderer {
  public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();
    bool initialize();
    void begin(const glm::mat4& viewProjection, glm::vec3 eye, glm::vec4 clipPlane = {0, 0, 0, 1});
    void box(glm::vec3 center, glm::vec3 size, glm::vec3 color, float yaw = 0);
    void quad(glm::vec3 center, glm::vec3 right, glm::vec3 up, glm::vec3 color);
    void triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color);
    void beginProjection(glm::vec3 origin, const glm::mat3& inverseRotation, glm::vec4 slideBounds,
                         float planeZ, const std::vector<float>& mask, int resolution,
                         const glm::mat3& artworkInverse, glm::vec4 artworkFrame);
    void projectionStrokes(float opacity = 1);
    void projectionWash(float opacity);
    void opacity(float value);
    void projectedStroke(glm::vec3 a, glm::vec3 b, glm::vec3 normal, float width, glm::vec3 color);
    void endProjection();
    void line(glm::vec3 a, glm::vec3 b, float width, glm::vec3 color);
    void flush();
    void label(const std::wstring& text, glm::vec3 center, glm::vec3 right, glm::vec3 up,
               glm::vec3 color = {1, 1, 1});
    void mirror(glm::vec3 center, glm::vec3 right, glm::vec3 up, const glm::mat4& reflectedViewProjection);
    void beginReflection();
    void endReflection(int width, int height);
    void overlay(int width, int height);
    void text(const std::wstring& value, float x, float y, float height,
              glm::vec3 color = {0.94f, 0.94f, 0.87f});
    void panel(float x, float y, float width, float height, glm::vec3 color);
    bool saveFrame(const std::string& path, int width, int height) const;
    static glm::vec3 reflectPoint(glm::vec3 p, glm::vec3 center, glm::vec3 normal);

  private:
    struct TextTexture {
        GLuint id = 0;
        float aspect = 1;
    };
    TextTexture& textureFor(const std::wstring& text);
    void drawTexture(GLuint texture, int mode, glm::vec3 center, glm::vec3 right, glm::vec3 up,
                     glm::vec3 color, const glm::mat4& reflected);
    GLuint program_ = 0, vertexArray_ = 0, vertexBuffer_ = 0;
    GLuint reflectionBuffer_ = 0, reflectionTexture_ = 0, reflectionDepth_ = 0;
    GLuint projectionMask_ = 0;
    GLuint artworkTexture_ = 0;
    std::vector<Vertex> vertices_;
    std::map<std::wstring, TextTexture> textTextures_;
};
} // namespace school
