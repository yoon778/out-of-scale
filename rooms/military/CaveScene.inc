constexpr Footprint clearing{{-10.20f, 7.30f}, {-5.20f, 13.20f}};
constexpr std::array cavePassages{
    Footprint{{-5.20f, 7.80f}, {1.40f, 10.40f}},
    Footprint{{-1.20f, 2.50f}, {1.40f, 10.40f}},
    Footprint{{-1.20f, 2.50f}, {7.25f, 5.10f}},
};
constexpr float caveCeiling = 2.95f;

struct CaveWall {
    glm::vec2 first;
    glm::vec2 last;
    glm::vec2 outside;
};

constexpr CaveWall caveWalls[]{
    {{-5.20f, 7.80f}, {-1.20f, 7.80f}, {0.0f, -1.0f}},
    {{-1.20f, 7.80f}, {-1.20f, 2.50f}, {-1.0f, 0.0f}},
    {{-1.20f, 2.50f}, {7.25f, 2.50f}, {0.0f, -1.0f}},
    {{7.25f, 5.10f}, {1.40f, 5.10f}, {0.0f, 1.0f}},
    {{1.40f, 5.10f}, {1.40f, 10.40f}, {1.0f, 0.0f}},
    {{1.40f, 10.40f}, {-5.20f, 10.40f}, {0.0f, 1.0f}},
};
constexpr float caveWallThickness = 0.28f;
constexpr float caveRoofThickness = 0.45f;

glm::vec3 cavePosition(const glm::vec2& point, float height) {
    return {point.x, height, point.y};
}

const auto caveRock = [] {
    std::array<Bounds, std::size(caveWalls) + cavePassages.size()> rock{};
    std::size_t index = 0;
    for (const auto& wall : caveWalls) {
        const auto thickness = caveWallThickness * wall.outside;
        const auto low = glm::min(wall.first, wall.last) + glm::min(thickness, glm::vec2{0.0f});
        const auto high = glm::max(wall.first, wall.last) + glm::max(thickness, glm::vec2{0.0f});
        rock[index++] = {cavePosition(low, 0.0f), cavePosition(high, caveCeiling + caveRoofThickness)};
    }
    for (const auto& passage : cavePassages) {
        rock[index++] = {cavePosition(passage.min, caveCeiling),
                         cavePosition(passage.max, caveCeiling + caveRoofThickness)};
    }
    return rock;
}();

float caveWallDepth(const glm::vec3& origin, const glm::vec3& direction, float maxDepth) {
    for (const auto& rock : caveRock) {
        const auto hit = perspective::rayBoxDistance(origin, direction, (rock.min + rock.max) * 0.5f,
                                                      (rock.max - rock.min) * 0.5f);
        if (hit && *hit < maxDepth) {
            maxDepth = *hit;
        }
    }
    return maxDepth;
}

bool caveRockIntersects(const Bounds& bounds) {
    return std::any_of(caveRock.begin(), caveRock.end(), [&](const Bounds& rock) {
        return glm::all(glm::lessThan(bounds.min, rock.max)) &&
               glm::all(glm::greaterThan(bounds.max, rock.min));
    });
}

struct CaveSurface {
    GLint first;
    GLsizei count;
    glm::vec3 center;
};

struct CaveScene {
    std::vector<MeshVertex> vertices;
    std::vector<CaveSurface> surfaces;
    GLsizei exteriorCount = 0;
};

void caveTriangle(CaveScene& cave, const glm::vec3& color, const glm::vec3& a,
                  const glm::vec3& b, const glm::vec3& c) {
    cave.vertices.insert(cave.vertices.end(), {{a, color}, {b, color}, {c, color}});
}

void caveQuad(CaveScene& cave, const glm::vec3& color, const glm::vec3& a,
              const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& middle) {
    caveTriangle(cave, color * 0.83f, a, b, middle);
    caveTriangle(cave, color * 0.96f, b, c, middle);
    caveTriangle(cave, color, c, d, middle);
    caveTriangle(cave, color * 0.89f, d, a, middle);
}

void caveSurface(CaveScene& cave, const glm::vec3& color, const glm::vec3& a,
                 const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                 const glm::vec3& facetOffset, bool interior) {
    const auto center = (a + b + c + d) * 0.25f;
    const auto first = static_cast<GLint>(cave.vertices.size());
    caveQuad(cave, color, a, b, c, d, center + facetOffset);
    if (interior) {
        cave.surfaces.push_back({first, 12, center});
    }
}

void caveFloorOrRoof(CaveScene& cave, const Footprint& bounds, float height,
                     const glm::vec3& color, bool interior) {
    const auto dimensions = bounds.max - bounds.min;
    const int columns = static_cast<int>(std::ceil(dimensions.x / 1.15f));
    const int rows = static_cast<int>(std::ceil(dimensions.y / 1.15f));
    const glm::vec2 cell = dimensions / glm::vec2{columns, rows};
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const auto lower = bounds.min + cell * glm::vec2{column, row};
            const auto upper = lower + cell;
            const glm::vec3 facetOffset{0.16f * cell.x, (height > 0.1f ? 0.12f : -0.022f),
                                         0.08f * cell.y};
            caveSurface(cave, color,
                        cavePosition(lower, height), {upper.x, height, lower.y},
                        cavePosition(upper, height), {lower.x, height, upper.y}, facetOffset, interior);
        }
    }
}

glm::vec2 caveOuterCorner(const CaveWall& wall, const glm::vec2& corner) {
    glm::vec2 outside = wall.outside;
    for (const auto& neighbor : caveWalls) {
        if (&neighbor != &wall && (neighbor.first == corner || neighbor.last == corner)) {
            outside += neighbor.outside;
            break;
        }
    }
    return corner + caveWallThickness * outside;
}

void caveWallFaces(CaveScene& cave, const CaveWall& wall, bool interior) {
    const auto start = interior ? wall.first : caveOuterCorner(wall, wall.first);
    const auto end = interior ? wall.last : caveOuterCorner(wall, wall.last);
    const auto span = end - start;
    const int pieces = static_cast<int>(std::ceil(glm::length(span) / 1.15f));
    constexpr float levels[]{0.0f, 0.73f, 1.96f, caveCeiling + caveRoofThickness};
    const auto outside = cavePosition(wall.outside, 0.0f);
    const auto stone = interior ? glm::vec3{0.37f, 0.40f, 0.45f} : glm::vec3{0.105f, 0.127f, 0.16f};
    for (int piece = 0; piece < pieces; ++piece) {
        const auto left = start + span * (static_cast<float>(piece) / pieces);
        const auto right = start + span * (static_cast<float>(piece + 1) / pieces);
        for (std::size_t level = 1; level < std::size(levels); ++level) {
            const float shade = 0.79f + (piece * 3 + level * 5) % 9 * 0.026f;
            const glm::vec3 facetOffset = outside * (0.065f + 0.055f * (piece % 3)) +
                                            glm::vec3{0.0f, 0.10f, 0.0f};
            caveSurface(cave, stone * shade,
                        cavePosition(left, levels[level - 1]), cavePosition(right, levels[level - 1]),
                        cavePosition(right, levels[level]), cavePosition(left, levels[level]),
                        facetOffset, interior);
        }
    }
}

void caveBoulder(CaveScene& cave, const glm::vec2& location, float radius, float height, float base = -0.09f) {
    constexpr glm::vec2 outline[]{
        {-0.87f, -0.24f}, {-0.40f, -0.85f}, {0.52f, -0.76f},
        {0.91f, 0.00f}, {0.39f, 0.76f}, {-0.56f, 0.70f},
    };
    constexpr glm::vec3 stone{0.11f, 0.14f, 0.18f};
    const auto crown = cavePosition(location + glm::vec2{0.07f, -0.12f} * radius, height);
    for (std::size_t index = 0; index < std::size(outline); ++index) {
        const auto a = location + outline[index] * radius;
        const auto b = location + outline[(index + 1) % std::size(outline)] * radius;
        const auto upperA = glm::mix(a, location, 0.22f);
        const auto upperB = glm::mix(b, location, 0.22f);
        const float shoulder = glm::mix(base, height, 0.47f + 0.05f * (index % 3));
        const float nextShoulder = glm::mix(base, height, 0.47f + 0.05f * ((index + 1) % std::size(outline) % 3));
        const auto c = cavePosition(upperA, shoulder);
        const auto d = cavePosition(upperB, nextShoulder);
        caveTriangle(cave, stone * 0.78f, cavePosition(a, base), cavePosition(b, base), c);
        caveTriangle(cave, stone * 0.87f, cavePosition(b, base), d, c);
        caveTriangle(cave, stone * (0.9f + index * 0.04f), c, d, crown);
    }
}

void clearingApron(CaveScene& cave, const glm::vec3& slate) {
    constexpr glm::vec2 corners[]{
        clearing.min, {clearing.max.x, clearing.min.y}, clearing.max, {clearing.min.x, clearing.max.y},
    };
    constexpr glm::vec2 slopeFoot[]{
        {-12.65f, 5.70f}, {-3.48f, 5.85f}, {-2.92f, 15.22f}, {-12.80f, 15.00f},
    };
    for (std::size_t index = 0; index < std::size(corners); ++index) {
        const auto next = (index + 1) % std::size(corners);
        caveSurface(cave, slate * 0.86f, cavePosition(corners[index], 0.016f),
                    cavePosition(corners[next], 0.016f), cavePosition(slopeFoot[next], -0.67f),
                    cavePosition(slopeFoot[index], -0.67f), {0.0f, 0.055f, 0.0f}, false);
    }
}

const CaveScene& caveScene() {
    static const CaveScene cave = [] {
        CaveScene scene;
        constexpr glm::vec3 slate{0.12f, 0.15f, 0.17f};
        constexpr glm::vec3 rock{0.15f, 0.18f, 0.22f};
        constexpr glm::vec3 tunnelFloor{0.35f, 0.38f, 0.42f};
        constexpr glm::vec3 tunnelCeiling{0.29f, 0.32f, 0.38f};
        // Split the overlapping passages into an exact, once-covered floor and ceiling.
        constexpr Footprint tunnelSections[]{
            {{-5.20f, 7.80f}, {-1.20f, 10.40f}},
            cavePassages[1],
            {{1.40f, 2.50f}, {7.25f, 5.10f}},
        };
        caveFloorOrRoof(scene, clearing, 0.016f, slate, false);
        clearingApron(scene, slate);
        for (const auto& section : tunnelSections) {
            caveFloorOrRoof(scene, section, caveCeiling + caveRoofThickness, rock, false);
        }
        for (const auto& wall : caveWalls) {
            caveWallFaces(scene, wall, false);
            constexpr float roofTop = caveCeiling + caveRoofThickness;
            caveSurface(scene, rock, cavePosition(wall.first, roofTop), cavePosition(wall.last, roofTop),
                        cavePosition(caveOuterCorner(wall, wall.last), roofTop),
                        cavePosition(caveOuterCorner(wall, wall.first), roofTop), {0.0f, 0.0f, 0.0f}, false);
        }
        for (float x : {-5.20f, 7.25f}) {
            const auto& passage = x < 0.0f ? cavePassages.front() : cavePassages.back();
            const float near = passage.min.y - caveWallThickness;
            const float far = passage.max.y + caveWallThickness;
            caveSurface(scene, rock, {x, caveCeiling, near}, {x, caveCeiling, far},
                        {x, caveCeiling + caveRoofThickness, far}, {x, caveCeiling + caveRoofThickness, near},
                        {x < 0.0f ? -0.13f : 0.13f, 0.08f, 0.12f}, false);
            for (float z : {near, far}) {
                const float edge = z < passage.min.y ? passage.min.y : passage.max.y;
                caveSurface(scene, rock, {x, 0.0f, z}, {x, 0.0f, edge},
                            {x, caveCeiling, edge}, {x, caveCeiling, z},
                            {x < 0.0f ? -0.12f : 0.12f, 0.12f, 0.0f}, false);
            }
        }
        caveBoulder(scene, {-10.05f, 8.08f}, 0.91f, 0.68f);
        caveBoulder(scene, {-10.15f, 11.91f}, 1.30f, 0.94f);
        caveBoulder(scene, {-8.78f, 13.05f}, 1.10f, 1.18f);
        caveBoulder(scene, {-6.93f, 13.02f}, 0.76f, 0.63f);
        caveBoulder(scene, {-7.98f, 7.34f}, 0.68f, 0.48f);
        caveBoulder(scene, {-5.33f, 7.41f}, 0.81f, 1.60f);
        caveBoulder(scene, {-5.15f, 10.67f}, 0.43f, 0.78f);
        caveBoulder(scene, {-5.05f, 8.18f}, 0.75f, 4.01f, 3.08f);
        caveBoulder(scene, {-4.97f, 9.04f}, 0.87f, 4.28f, 3.17f);
        caveBoulder(scene, {-4.81f, 10.05f}, 0.68f, 3.85f, 3.19f);
        caveBoulder(scene, {6.97f, 2.95f}, 0.62f, 3.92f, 3.16f);
        caveBoulder(scene, {7.01f, 4.57f}, 0.67f, 4.02f, 3.19f);
        scene.exteriorCount = static_cast<GLsizei>(scene.vertices.size());

        for (const auto& section : tunnelSections) {
            caveFloorOrRoof(scene, section, 0.018f, tunnelFloor, true);
            caveFloorOrRoof(scene, section, caveCeiling, tunnelCeiling, true);
        }
        for (const auto& wall : caveWalls) {
            caveWallFaces(scene, wall, true);
        }
        return scene;
    }();
    return cave;
}

float caveLight(const glm::vec3& point, const std::optional<glm::vec3>& moon, float strength) {
    if (!moon) {
        return 0.0f;
    }
    const auto toPoint = point - *moon;
    const float distance = glm::length(toPoint);
    const float reach = 4.1f + 0.55f * strength;
    if (distance >= reach || caveWallDepth(*moon, toPoint, 0.993f) < 0.993f) {
        return 0.0f;
    }
    return 1.0f - glm::smoothstep(0.40f, reach, distance);
}

void drawCaveScene(const BoxRenderer& renderer, GLint first, const glm::vec3& eye,
                   const std::optional<glm::vec3>& moon, float strength) {
    const auto& cave = caveScene();
    constexpr glm::mat4 model{1.0f};
    renderer.drawMovableMesh(first, cave.exteriorCount, model, glm::vec3{1.0f});

    constexpr glm::vec3 dark{0.0f};
    // Cave surfaces have zero ambient light because the two bends block the sky.
    if (!moon || glm::length(*moon - eye) > 13.0f) {
        renderer.drawMovableMesh(first + cave.exteriorCount,
                                 static_cast<GLsizei>(cave.vertices.size()) - cave.exteriorCount, model, dark);
    } else {
        constexpr glm::vec3 exposed{0.94f, 1.10f, 1.35f};
        for (const auto& surface : cave.surfaces) {
            const float light = caveLight(surface.center, moon, strength);
            renderer.drawMovableMesh(first + surface.first, surface.count, model, exposed * light);
        }
    }

    constexpr glm::vec3 entranceTape{-5.02f, 1.10f, 10.35f};
    constexpr glm::vec3 tapeLengthAcrossX{0.29f, 0.14f, 0.045f};
    constexpr glm::vec3 tapeLengthAcrossZ{0.045f, 0.14f, 0.29f};
    constexpr glm::vec3 brightTape{0.67f, 0.84f, 1.0f};
    const auto tape = [&](const glm::vec3& center, const glm::vec3& size, float ambient = 0.0f) {
        const float light = std::clamp(1.65f * caveLight(center, moon, strength), ambient, 1.0f);
        renderer.drawBox(center, size, brightTape * light);
    };
    tape(entranceTape, tapeLengthAcrossX, 0.11f);
    tape({1.33f, 1.15f, 9.1f}, tapeLengthAcrossZ);
    tape({1.33f, 1.15f, 7.1f}, tapeLengthAcrossZ);
    tape({0.08f, 1.15f, 2.57f}, tapeLengthAcrossX);
    tape({2.10f, 1.15f, 2.57f}, tapeLengthAcrossX);
    tape({5.40f, 1.15f, 2.57f}, tapeLengthAcrossX);
}
