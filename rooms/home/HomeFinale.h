#pragma once

#include <GL/glm/glm.hpp>

#include <array>
#include <cstddef>
#include <optional>

namespace home_finale {
inline constexpr float hallCenterX = 3.75f;
inline constexpr float left = -10.25f;
inline constexpr float right = 17.75f;
inline constexpr float entranceZ = 6.60f;
inline constexpr float frontZ = 8.65f;
inline constexpr float backZ = 34.65f;
inline constexpr float entranceHalfWidth = 2.05f;
inline constexpr float ceiling = 22.0f;
inline constexpr float exitHeight = 15.0f;
inline constexpr float exitHalfWidth = 1.45f;
inline constexpr float exitZ = 35.35f;
inline constexpr float exitDoorHeight = 2.65f;

struct Box {
    glm::vec3 center;
    glm::vec3 size;
};

inline constexpr std::array rests{
    Box{{-.90f, 3.32f, 31.65f}, {4.10f, .36f, 4.20f}},
    Box{{3.75f, 7.22f, 31.65f}, {4.10f, .36f, 4.20f}},
    Box{{8.40f, 11.02f, 31.65f}, {4.10f, .36f, 4.20f}},
};
inline constexpr Box exitLanding{{hallCenterX, exitHeight - .20f, 32.90f}, {4.65f, .40f, 3.50f}};
inline constexpr Box exitSill{{hallCenterX, exitHeight - .18f, 35.325f}, {2 * exitHalfWidth, .36f, 1.35f}};

enum class Region { School, Military, Office, Home };
enum class Kind {
    Desk, Book, Chair, Board,
    Locker, Crate, Trunk, Bed,
    Cabinet, PaperBox, Console, Table,
    Shelf, Cushion, Drawer, Case,
};

struct Prop {
    Region region;
    Kind kind;
    glm::vec3 baseSize;
    glm::vec3 center;
    float scale = 1.0f;
    int turns = 0;

    glm::vec3 size() const {
        if (turns % 2 != 0) return glm::vec3{baseSize.z, baseSize.y, baseSize.x} * scale;
        return baseSize * scale;
    }
};

class Room {
public:
    bool interact(glm::vec3 eye, glm::vec3 direction);
    void scroll(double yOffset);
    void rotate();
    void update(float elapsed, glm::vec3 eye, glm::vec3 direction);
    glm::vec3 resolveMovement(glm::vec3 previous, glm::vec3 desired, bool* grounded);

    bool isCarrying() const { return held_.has_value(); }
    bool exitOpen() const { return exitOpen_; }
    bool escaped() const { return escaped_; }
    float doorProgress() const { return doorProgress_; }
    bool placementReady() const { return placement_.has_value(); }
    std::optional<std::size_t> heldIndex() const { return held_; }
    const std::array<Prop, 20>& props() const { return props_; }

private:
    struct SightHit {
        float distance;
        std::optional<std::size_t> prop;
        std::optional<Box> box;
    };

    SightHit sight(glm::vec3 eye, glm::vec3 direction, float limit) const;
    std::optional<Prop> placement(glm::vec3 eye, glm::vec3 direction) const;
    bool placeable(const Prop& prop, glm::vec3 eye) const;

    std::array<Prop, 20> props_{{
        {Region::School, Kind::Desk, {1.42f, .78f, .82f}, {-7.90f, .39f, 12.80f}},
        {Region::School, Kind::Chair, {.64f, .92f, .70f}, {-7.40f, .46f, 15.80f}},
        {Region::School, Kind::Book, {1.11f, .36f, .89f}, {-7.95f, .18f, 18.45f}},
        {Region::School, Kind::Board, {1.78f, 1.28f, .48f}, {-5.80f, .64f, 16.60f}},
        {Region::School, Kind::Desk, {1.42f, .78f, .82f}, {-5.70f, .39f, 18.10f}},
        {Region::Military, Kind::Locker, {.84f, 1.56f, .86f}, {-6.35f, .78f, 28.90f}},
        {Region::Military, Kind::Crate, {1.08f, .72f, .86f}, {-3.80f, .36f, 29.10f}},
        {Region::Military, Kind::Trunk, {1.24f, .62f, .82f}, {-1.35f, .31f, 27.65f}},
        {Region::Military, Kind::Bed, {1.76f, .63f, .92f}, {-6.40f, .315f, 32.20f}},
        {Region::Military, Kind::Crate, {1.08f, .72f, .86f}, {-4.10f, .36f, 31.75f}},
        {Region::Office, Kind::Cabinet, {1.08f, 1.28f, .78f}, {15.40f, .64f, 26.95f}},
        {Region::Office, Kind::PaperBox, {.94f, .58f, .77f}, {13.30f, .29f, 25.50f}},
        {Region::Office, Kind::Console, {1.18f, .76f, .89f}, {15.10f, .38f, 24.70f}},
        {Region::Office, Kind::Table, {1.66f, .84f, 1.00f}, {15.40f, .42f, 29.80f}},
        {Region::Office, Kind::PaperBox, {.94f, .58f, .77f}, {14.90f, .29f, 32.40f}},
        {Region::Home, Kind::Shelf, {1.25f, 1.60f, .81f}, {6.85f, .80f, 11.45f}},
        {Region::Home, Kind::Cushion, {1.04f, .39f, .92f}, {9.10f, .195f, 11.55f}},
        {Region::Home, Kind::Drawer, {1.15f, .86f, .88f}, {11.45f, .43f, 11.45f}},
        {Region::Home, Kind::Case, {1.15f, .60f, .77f}, {13.85f, .30f, 10.25f}},
        {Region::Home, Kind::Table, {1.66f, .84f, 1.00f}, {8.60f, .42f, 15.20f}},
    }};
    std::array<float, 20> verticalSpeeds_{};
    std::optional<std::size_t> held_;
    std::optional<Prop> placement_;
    float carryRatio_ = 1.0f;
    float wheelScale_ = 1.0f;
    bool exitOpen_ = false;
    bool escaped_ = false;
    float doorProgress_ = 0.0f;
};
} // namespace home_finale
