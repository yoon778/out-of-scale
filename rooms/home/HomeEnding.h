#pragma once

#include <algorithm>

class HomeEnding {
public:
    void update(float elapsed, bool escaped) {
        if (escaped && elapsed > 0.0f) {
            elapsed_ = std::min(duration, elapsed_ + elapsed);
        }
    }

    float fade() const {
        const float progress = std::clamp((elapsed_ - daylightHold) / fadeDuration, 0.0f, 1.0f);
        return progress * progress * (3.0f - 2.0f * progress);
    }

    static constexpr float daylightHold = 1.1f;
    static constexpr float fadeDuration = 2.2f;
    static constexpr float duration = daylightHold + fadeDuration;

private:
    float elapsed_ = 0.0f;
};
