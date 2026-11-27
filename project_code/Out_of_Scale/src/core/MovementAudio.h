#pragma once

#include <GL/glm/glm.hpp>

#include <algorithm>
#include <cmath>

class MovementAudio {
public:
    enum class Cue { None, Step, Land };

    void reset(const glm::vec3& position, bool grounded) {
        previous_ = position;
        grounded_ = grounded;
        peak_ = position.y;
        distanceToStep_ = firstStepDistance;
    }

    Cue sample(const glm::vec3& position, bool grounded) {
        const glm::vec3 change = position - previous_;
        const float distance = std::hypot(change.x, change.z);
        if (distance > 1.1f || std::abs(change.y) > 1.1f) {
            reset(position, grounded);
            return Cue::None;
        }

        const bool wasGrounded = grounded_;
        peak_ = wasGrounded ? std::max(previous_.y, position.y) : std::max(peak_, position.y);
        previous_ = position;
        grounded_ = grounded;
        if (!grounded || !wasGrounded) {
            distanceToStep_ = firstStepDistance;
            return grounded && peak_ - position.y >= 0.18f ? Cue::Land : Cue::None;
        }
        distanceToStep_ -= distance;
        if (distanceToStep_ > 0.0f) {
            return Cue::None;
        }
        distanceToStep_ += 1.25f;
        return Cue::Step;
    }

private:
    static constexpr float firstStepDistance = 0.6f;
    glm::vec3 previous_{};
    bool grounded_ = false;
    float peak_ = 0.0f;
    float distanceToStep_ = firstStepDistance;
};
