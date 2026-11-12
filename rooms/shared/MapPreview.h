#pragma once

#include "BoxRenderer.h"

class MapPreview {
public:
    enum class Theme { School, Office, Home };

    explicit MapPreview(Theme theme) : theme_(theme) {}

    bool initialize() { return renderer_.initialize(); }

    const char* title() const {
        switch (theme_) {
        case Theme::School: return "Out of Scale | F1 School | Preview";
        case Theme::Office: return "Out of Scale | F3 Office | Preview";
        case Theme::Home: return "Out of Scale | F4 Home | Preview";
        }
        return "Out of Scale | Preview";
    }

    void draw(const glm::mat4& viewProjection, const glm::vec3&, const glm::vec3&) const {
        constexpr float width = halfSize * 2.0f;
        constexpr float thickness = 0.14f;
        constexpr float wallCenter = halfSize + thickness * 0.5f;
        constexpr float height = 3.2f;
        constexpr float sides[]{-1.0f, 1.0f};
        const glm::vec3 wallTint = theme_ == Theme::School ? glm::vec3{0.77f, 0.76f, 0.60f}
            : theme_ == Theme::Office ? glm::vec3{0.58f, 0.67f, 0.75f}
                                      : glm::vec3{0.81f, 0.62f, 0.48f};

        renderer_.begin(viewProjection);
        renderer_.drawBox({0.0f, -0.08f, 0.0f}, {width, 0.16f, width}, {0.45f, 0.43f, 0.39f});
        for (float side : sides) {
            renderer_.drawBox({0.0f, height * 0.5f, side * wallCenter}, {width, height, thickness}, wallTint);
            renderer_.drawBox({side * wallCenter, height * 0.5f, 0.0f}, {thickness, height, width}, wallTint);
        }

        switch (theme_) {
        case Theme::School:
            marker(0.0f, 1.78f, 4.70f, 1.53f, {0.39f, 0.24f, 0.14f});
            marker(0.0f, 1.78f, 4.54f, 1.37f, {0.09f, 0.25f, 0.20f}, 0.09f);
            for (int step = 0; step < 3; ++step) {
                marker(-0.66f + 0.66f * step, 1.45f + 0.29f * step, 0.66f, 0.04f,
                       {0.84f, 0.89f, 0.80f}, 0.12f);
            }
            break;
        case Theme::Office:
            marker(0.0f, 1.12f, 1.36f, 2.24f, {0.12f, 0.20f, 0.28f});
            marker(0.0f, 1.09f, 1.20f, 2.18f, {0.26f, 0.36f, 0.45f}, 0.09f);
            marker(0.0f, 2.48f, 0.75f, 0.23f, {0.20f, 0.65f, 0.38f});
            marker(2.30f, 1.76f, 2.32f, 1.03f, {0.16f, 0.28f, 0.38f});
            marker(2.30f, 1.76f, 2.17f, 0.88f, {0.52f, 0.73f, 0.81f}, 0.09f);
            break;
        case Theme::Home:
            marker(-1.14f, 1.00f, 0.57f, 0.78f, {0.39f, 0.23f, 0.15f});
            marker(-1.14f, 1.00f, 0.45f, 0.66f, {0.62f, 0.38f, 0.20f}, 0.09f);
            marker(-1.01f, 0.98f, 0.05f, 0.05f, {0.90f, 0.72f, 0.35f}, 0.12f);
            marker(1.15f, 1.85f, 2.04f, 1.27f, {0.61f, 0.39f, 0.27f});
            marker(1.15f, 1.85f, 1.88f, 1.11f, {0.33f, 0.43f, 0.60f}, 0.09f);
            marker(1.15f, 1.85f, 0.06f, 1.11f, {0.69f, 0.49f, 0.34f}, 0.12f);
            break;
        }
    }

    static constexpr float halfSize = 5.0f;

private:
    void marker(float x, float y, float width, float height, const glm::vec3& color,
                float depth = 0.06f) const {
        renderer_.drawBox({x, y, -halfSize + depth * 0.5f}, {width, height, depth}, color);
    }

    Theme theme_;
    BoxRenderer renderer_;
};
