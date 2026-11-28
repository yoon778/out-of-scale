#pragma once

#include "HomeFinale.h"
#include "../shared/BoxRenderer.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

namespace home_finale {
namespace visual_detail {
struct Range {
    GLint first;
    GLsizei count;
};

inline constexpr std::size_t kindCount = static_cast<std::size_t>(Kind::Case) + 1;
inline constexpr float exitDoorHeight = 2.70f;
inline constexpr glm::vec3 wall{.33f, .39f, .41f};
inline constexpr glm::vec3 column{.30f, .35f, .37f};
inline constexpr glm::vec3 darkTrim{.18f, .24f, .26f};
inline constexpr glm::vec3 bronze{.79f, .64f, .42f};
inline constexpr glm::vec3 ivory{.93f, .89f, .73f};
inline constexpr glm::vec3 oak{.57f, .37f, .23f};
inline constexpr glm::vec3 sage{.36f, .47f, .36f};
inline constexpr glm::vec3 sky{.72f, .80f, .82f};

inline void box(std::vector<MeshVertex>& vertices, glm::vec3 center, glm::vec3 size, glm::vec3 color) {
    constexpr std::array<glm::vec3, 8> corners{{
        {-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f},
        {-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f},
    }};
    constexpr int indices[6][6]{{0,3,2,0,2,1}, {4,5,6,4,6,7}, {0,4,7,0,7,3},
                                {1,2,6,1,6,5}, {0,1,5,0,5,4}, {3,7,6,3,6,2}};
    constexpr float light[]{.88f,.78f,.81f,.95f,.63f,1.0f};
    for (std::size_t face = 0; face < std::size(indices); ++face) {
        for (int index : indices[face])
            vertices.push_back({center + corners[index] * size, color * light[face]});
    }
}

inline void surface(std::vector<MeshVertex>& vertices, const Box& bounds, glm::vec3 color) {
    box(vertices, bounds.center, bounds.size, color);
}

inline void classroom(std::vector<MeshVertex>& vertices) {
    constexpr glm::vec3 plaster{.67f, .72f, .57f};
    constexpr glm::vec3 greenBoard{.22f, .38f, .34f};
    const float x = left;
    box(vertices, {x + .02f,2.46f,21.4f}, {.08f,4.92f,23.8f}, plaster);
    box(vertices, {x + .073f,.77f,21.4f}, {.028f,1.54f,23.8f}, {.43f,.52f,.38f});
    box(vertices, {x + .105f,1.53f,21.4f}, {.08f,.078f,23.8f}, oak);
    box(vertices, {x + .12f,2.48f,16.95f}, {.19f,2.05f,6.65f}, oak);
    box(vertices, {x + .226f,2.50f,16.95f}, {.027f,1.88f,6.44f}, greenBoard);
    box(vertices, {x + .28f,1.49f,16.95f}, {.29f,.07f,6.68f}, bronze);
    for (int i = 0; i < 4; ++i) {
        const float z = 14.73f + static_cast<float>(i) * .77f;
        box(vertices, {x + .247f,2.54f,z}, {.016f,.018f,.44f}, {.67f,.75f,.63f});
        box(vertices, {x + .247f,2.50f,z + .21f}, {.016f,.10f,.018f}, {.67f,.75f,.63f});
    }
    for (float z : {23.25f, 28.03f}) {
        box(vertices, {x + .09f,2.75f,z}, {.14f,2.40f,3.86f}, ivory);
        box(vertices, {x + .171f,2.80f,z}, {.035f,2.16f,3.60f}, sky);
        box(vertices, {x + .194f,2.80f,z}, {.037f,2.16f,.095f}, ivory);
        box(vertices, {x + .194f,2.45f,z}, {.037f,.085f,3.60f}, ivory);
        box(vertices, {x + .22f,1.64f,z}, {.42f,.105f,3.91f}, ivory);
    }
}

inline void barracks(std::vector<MeshVertex>& vertices) {
    const float z = backZ;
    constexpr glm::vec3 olive{.46f, .51f, .36f};
    constexpr glm::vec3 steel{.65f, .67f, .60f};
    box(vertices, {hallCenterX,2.50f,z - .032f}, {26.55f,5.0f,.07f}, olive);
    box(vertices, {hallCenterX,.53f,z - .082f}, {26.55f,1.06f,.042f}, sage * .72f);
    box(vertices, {hallCenterX,4.95f,z - .12f}, {26.55f,.15f,.18f}, bronze * .72f);
    for (const float x : {-6.45f,-3.35f,-.25f,2.85f,5.95f,9.05f,12.15f}) {
        box(vertices, {x,3.68f,z - .096f}, {2.80f,1.58f,.15f}, steel);
        box(vertices, {x,3.68f,z - .184f}, {2.54f,1.34f,.034f}, {.26f,.34f,.37f});
        box(vertices, {x,3.54f,z - .213f}, {2.53f,.038f,.025f}, steel);
        box(vertices, {x,3.70f,z - .213f}, {.083f,1.34f,.025f}, steel);
    }
    for (const float x : {-5.9f,-1.35f,3.25f,7.9f,12.5f})
        box(vertices, {x,5.04f,z - .58f}, {2.70f,.096f,.22f}, ivory * .92f);
}

inline void office(std::vector<MeshVertex>& vertices) {
    constexpr glm::vec3 blueSteel{.40f,.53f,.58f};
    constexpr glm::vec3 glass{.28f,.45f,.54f};
    const float x = right;
    box(vertices, {x - .04f,2.61f,21.4f}, {.1f,5.22f,23.8f}, blueSteel);
    for (int bay = 0; bay < 5; ++bay) {
        const float z = 11.95f + static_cast<float>(bay) * 4.72f;
        box(vertices, {x - .105f,2.85f,z}, {.13f,3.46f,4.43f}, darkTrim);
        box(vertices, {x - .179f,2.85f,z}, {.024f,3.20f,4.15f}, glass);
        box(vertices, {x - .20f,2.87f,z + .01f}, {.042f,3.2f,.07f}, blueSteel);
        box(vertices, {x - .212f,3.83f,z}, {.032f,.045f,4.16f}, sky * .74f);
        box(vertices, {x - .212f,1.73f,z}, {.032f,.045f,4.16f}, sky * .64f);
    }
    box(vertices, {x - .11f,.52f,21.4f}, {.16f,1.04f,23.8f}, darkTrim);
    box(vertices, {x - .32f,4.77f,21.4f}, {.36f,.055f,23.1f}, ivory);
}

inline void bedroom(std::vector<MeshVertex>& vertices) {
    constexpr glm::vec3 plaster{.77f,.66f,.54f};
    const float z = frontZ;
    const float segment = hallCenterX - entranceHalfWidth - left - .13f;
    for (float side : {-1.0f,1.0f}) {
        const float x = hallCenterX + side * (entranceHalfWidth + segment * .5f);
        box(vertices, {x,2.24f,z + .035f}, {segment,4.48f,.082f}, plaster);
        box(vertices, {x,.77f,z + .09f}, {segment,1.54f,.031f}, oak);
        box(vertices, {x,1.55f,z + .127f}, {segment,.083f,.14f}, bronze * .81f);
        for (int i = 0; i < 3; ++i) {
            const float frameX = x + (static_cast<float>(i) - 1.0f) * 2.10f;
            const float height = i == 1 ? 1.15f : .86f;
            box(vertices, {frameX,2.63f,z + .135f}, {1.51f,height,.15f}, oak * .66f);
            box(vertices, {frameX,2.63f,z + .22f}, {1.32f,height - .16f,.024f}, ivory);
            box(vertices, {frameX,2.61f,z + .242f}, {.94f,height * .42f,.020f}, sage * .89f);
            box(vertices, {frameX + .21f,2.78f,z + .255f}, {.18f,.17f,.014f}, bronze);
        }
    }
}

inline void architecture(std::vector<MeshVertex>& vertices) {
    constexpr float hallWidth = right - left;
    constexpr float hallLength = backZ - frontZ;
    constexpr float midZ = (backZ + frontZ) * .5f;
    constexpr float entryBegin = entranceZ + .04f;
    constexpr float doorTop = exitHeight + exitDoorHeight;
    box(vertices, {hallCenterX,-.13f,midZ}, {hallWidth,.26f,hallLength}, {.33f,.36f,.35f});
    box(vertices, {hallCenterX,-.12f,(entryBegin + frontZ) * .5f},
        {2 * entranceHalfWidth,.24f,frontZ - entryBegin}, oak * .70f);
    box(vertices, {hallCenterX,ceiling + .10f,midZ}, {hallWidth+.30f,.20f,hallLength+.30f}, {.31f,.37f,.39f});
    box(vertices, {left - .13f,ceiling * .5f,midZ}, {.26f,ceiling,hallLength}, wall);
    box(vertices, {right + .13f,ceiling * .5f,midZ}, {.26f,ceiling,hallLength}, wall);
    box(vertices, {hallCenterX,exitHeight * .5f,backZ + .07f}, {hallWidth,exitHeight,.25f}, wall);
    box(vertices, {hallCenterX,(ceiling + doorTop) * .5f,backZ + .07f},
        {hallWidth,ceiling - doorTop,.25f}, wall);
    constexpr float farSideWidth = hallWidth * .5f - exitHalfWidth;
    constexpr float nearSideWidth = hallWidth * .5f - entranceHalfWidth;
    for (float side : {-1.0f,1.0f}) {
        box(vertices, {hallCenterX+side*(exitHalfWidth+farSideWidth*.5f),exitHeight+exitDoorHeight*.5f,backZ+.07f},
            {farSideWidth,exitDoorHeight,.25f}, wall);
        box(vertices, {hallCenterX+side*(entranceHalfWidth+nearSideWidth*.5f),ceiling*.5f,frontZ-.11f},
            {nearSideWidth,ceiling,.22f}, wall);
        box(vertices, {hallCenterX+side*(entranceHalfWidth-.065f),1.45f,frontZ+.08f},
            {.13f,2.9f,.33f}, oak * .68f);
        box(vertices, {hallCenterX+side*(exitHalfWidth+.07f),exitHeight+exitDoorHeight*.5f,backZ-.20f},
            {.24f,exitDoorHeight,.45f}, bronze);
    }
    box(vertices, {hallCenterX,(ceiling+2.90f)*.5f,frontZ-.11f},
        {2*entranceHalfWidth,ceiling-2.90f,.22f}, wall);
    box(vertices, {hallCenterX,2.942f,frontZ+.08f}, {2*entranceHalfWidth+.25f,.15f,.33f}, oak * .68f);

    classroom(vertices);
    barracks(vertices);
    office(vertices);
    bedroom(vertices);

    for (float z : {frontZ+.21f,backZ-.28f}) {
        for (float x : {left+.22f,right-.22f}) {
            box(vertices, {x,ceiling*.5f,z}, {.44f,ceiling,.44f}, column);
            box(vertices, {x,5.18f,z}, {.56f,.16f,.56f}, bronze * .68f);
        }
    }
    for (float z : {13.35f,19.0f,24.65f,30.30f}) {
        box(vertices, {hallCenterX,ceiling-.27f,z}, {hallWidth,.44f,.44f}, darkTrim);
        box(vertices, {hallCenterX,ceiling-.51f,z}, {hallWidth-4.2f,.075f,.19f}, ivory * .91f);
        for (float x : {left+.09f,right-.09f}) {
            box(vertices, {x,13.0f,z}, {.21f,15.4f,.40f}, column);
        }
    }
    // Faint bands establish scale even where no furniture reaches the upper walls.
    for (float y : {5.35f,10.75f,16.45f}) {
        for (float x : {left+.065f,right-.065f})
            box(vertices, {x,y,midZ}, {.13f,.077f,hallLength-.50f}, bronze * .50f);
    }
    for (float z : {12.8f,17.6f,22.4f,27.2f,32.0f}) {
        box(vertices, {hallCenterX,.001f,z}, {hallWidth-.45f,.002f,.018f}, darkTrim * .93f);
    }
    for (float x : {-7.0f,-2.0f,3.0f,8.0f,13.0f}) {
        box(vertices, {x,.0012f,midZ}, {.016f,.002f,hallLength-.40f}, darkTrim * .93f);
    }
    for (const auto& rest : rests) {
        surface(vertices, rest, darkTrim);
        box(vertices, {rest.center.x,rest.center.y,rest.center.z-rest.size.z*.5f-.018f},
            {rest.size.x,.065f,.045f}, bronze * .78f);
    }
    surface(vertices, exitLanding, {.55f,.44f,.32f});
    surface(vertices, exitSill, {.78f,.64f,.43f});
    box(vertices, {hallCenterX,doorTop+.11f,backZ-.19f},
        {2*exitHalfWidth+.34f,.24f,.43f}, bronze);
    box(vertices, {hallCenterX,doorTop+.37f,backZ-.20f}, {1.32f,.12f,.12f}, ivory);
    for (float side : {-1.0f,1.0f}) {
        box(vertices, {hallCenterX+side*(exitHalfWidth+.13f),exitHeight+1.35f,exitZ+.04f},
            {.18f,2.70f,1.23f}, {.72f,.66f,.50f});
    }
    box(vertices, {hallCenterX,exitHeight+2.72f,exitZ+.06f}, {2*exitHalfWidth,.12f,1.25f}, ivory);
    box(vertices, {hallCenterX,exitHeight+1.34f,exitZ+.64f}, {2*exitHalfWidth,2.68f,.055f}, {1.0f,.90f,.67f});
}

inline void locker(std::vector<MeshVertex>& vertices, glm::vec3 body, glm::vec3 face, int doors) {
    box(vertices, {0,0,.012f}, {1,1,.976f}, body);
    for (int i = 0; i < doors; ++i) {
        const float width = .92f / static_cast<float>(doors);
        const float x = -.46f + (static_cast<float>(i) + .5f) * width;
        box(vertices, {x,.01f,-.488f}, {width - .022f,.91f,.012f}, face);
        box(vertices, {x,.23f,-.498f}, {width*.42f,.025f,.004f}, body*.64f);
        box(vertices, {x,.17f,-.498f}, {width*.42f,.021f,.004f}, body*.64f);
        box(vertices, {x + width*.28f,-.02f,-.497f}, {.022f,.14f,.006f}, bronze);
    }
}

inline void propArt(std::vector<MeshVertex>& vertices, Kind kind) {
    constexpr glm::vec3 linen{.83f,.79f,.61f};
    constexpr glm::vec3 navy{.29f,.39f,.45f};
    constexpr glm::vec3 olive{.49f,.55f,.37f};
    switch (kind) {
    case Kind::Desk:
    case Kind::Table:
    case Kind::Chair: {
        const glm::vec3 top = kind == Kind::Table ? navy * 1.19f : oak;
        const glm::vec3 legs = kind == Kind::Chair ? oak*.68f : darkTrim;
        box(vertices, {0,.38f,.006f}, {1,.24f,.988f}, top);
        box(vertices, {0,-.035f,.08f}, {.86f,.64f,.53f}, legs);
        box(vertices, {0,-.42f,0}, {.92f,.16f,.93f}, legs * 1.16f);
        box(vertices, {0,.38f,-.493f}, {.92f,.065f,.014f}, bronze);
        break;
    }
    case Kind::Book:
        box(vertices, {0,0,0}, {.94f,.80f,.94f}, linen);
        box(vertices, {-.425f,0,0}, {.15f,1,1}, {.41f,.37f,.51f});
        for (float y : {-.45f,.45f})
            box(vertices, {0,y,0}, {1,.10f,1}, {.41f,.37f,.51f});
        for (float y : {-.30f,-.14f,.03f,.19f,.32f})
            box(vertices, {.475f,y,0}, {.010f,.020f,.87f}, linen*.80f);
        break;
    case Kind::Board:
        box(vertices, {0,0,.008f}, {1,1,.984f}, oak*.87f);
        box(vertices, {0,0,-.491f}, {.90f,.87f,.012f}, {.19f,.35f,.31f});
        box(vertices, {-.15f,-.21f,-.499f}, {.23f,.013f,.002f}, ivory*.82f);
        box(vertices, {.12f,.11f,-.499f}, {.30f,.018f,.002f}, ivory*.82f);
        break;
    case Kind::Locker:
        locker(vertices, {.53f,.59f,.51f}, {.70f,.74f,.66f}, 2);
        break;
    case Kind::Cabinet:
        locker(vertices, {.33f,.46f,.51f}, {.46f,.62f,.67f}, 2);
        break;
    case Kind::Shelf:
        locker(vertices, oak*.75f, {.53f,.41f,.30f}, 2);
        for (float y : {-.25f,0.0f,.25f})
            box(vertices, {0,y,-.497f}, {.87f,.044f,.006f}, oak * 1.30f);
        break;
    case Kind::Drawer:
        box(vertices, {0,0,.012f}, {1,1,.976f}, oak*.85f);
        for (float y : {-.31f,0.0f,.31f}) {
            box(vertices, {0,y,-.49f}, {.93f,.282f,.014f}, oak*1.15f);
            box(vertices, {0,y,-.499f}, {.21f,.054f,.002f}, bronze);
        }
        break;
    case Kind::Bed:
        box(vertices, {0,-.21f,0}, {1,.58f,1}, sage*.85f);
        box(vertices, {0,.22f,0}, {.98f,.51f,.98f}, olive);
        box(vertices, {0,.488f,-.23f}, {.92f,.024f,.32f}, linen);
        box(vertices, {0,.493f,.22f}, {.95f,.014f,.10f}, sage*1.28f);
        break;
    case Kind::Cushion:
        box(vertices, {0,-.012f,.010f}, {1,.976f,.980f}, {.63f,.41f,.35f});
        box(vertices, {0,.491f,0}, {.86f,.018f,.86f}, {.79f,.57f,.46f});
        box(vertices, {0,-.024f,-.499f}, {1,.035f,.002f}, bronze);
        break;
    case Kind::Console:
        box(vertices, {0,0,.012f}, {1,1,.976f}, darkTrim);
        box(vertices, {0,.13f,-.487f}, {.78f,.52f,.020f}, navy);
        box(vertices, {0,.175f,-.497f}, {.62f,.365f,.006f}, {.37f,.61f,.63f});
        box(vertices, {.28f,-.25f,-.498f}, {.07f,.048f,.004f}, ivory);
        break;
    case Kind::Crate:
    case Kind::Trunk:
    case Kind::PaperBox:
    case Kind::Case: {
        glm::vec3 body = olive;
        if (kind == Kind::PaperBox) body = {.70f,.58f,.41f};
        if (kind == Kind::Case) body = {.43f,.35f,.43f};
        if (kind == Kind::Crate) body = oak;
        box(vertices, {0,-.05f,.009f}, {1,.90f,.982f}, body);
        box(vertices, {0,.444f,0}, {.99f,.102f,.99f}, body * 1.15f);
        for (float x : {-.36f,.36f}) {
            box(vertices, {x,0,-.498f}, {.08f,.86f,.004f}, body*.69f);
            box(vertices, {x,.499f,0}, {.08f,.002f,1.0f}, body*.69f);
        }
        box(vertices, {0,.10f,-.498f}, {.25f,.15f,.004f},
            kind == Kind::PaperBox ? linen : bronze * .77f);
        break;
    }
    }
}

struct VisualMesh {
    std::vector<MeshVertex> vertices;
    Range hall{}, door{};
    std::array<Range,kindCount> kinds{};
};

inline const VisualMesh& mesh() {
    static const VisualMesh result = [] {
        VisualMesh visual;
        auto append = [&](const auto& assemble) {
            const auto first = visual.vertices.size();
            assemble(visual.vertices);
            return Range{static_cast<GLint>(first),static_cast<GLsizei>(visual.vertices.size()-first)};
        };
        visual.hall = append(architecture);
        visual.door = append([](auto& vertices) {
            box(vertices, {exitHalfWidth,exitDoorHeight*.5f,0},
                {2*exitHalfWidth-.08f,exitDoorHeight-.10f,.15f}, oak);
            box(vertices, {exitHalfWidth,exitDoorHeight*.61f,-.086f},
                {2*exitHalfWidth-.40f,exitDoorHeight*.51f,.025f}, sage*.94f);
            box(vertices, {2*exitHalfWidth-.33f,.99f,-.12f}, {.22f,.10f,.08f}, bronze);
            box(vertices, {exitHalfWidth,.12f,-.087f}, {2*exitHalfWidth-.22f,.135f,.032f}, bronze*.68f);
        });
        for (std::size_t i = 0; i < visual.kinds.size(); ++i)
            visual.kinds[i] = append([&](auto& vertices) { propArt(vertices,static_cast<Kind>(i)); });
        return visual;
    }();
    return result;
}
} // namespace visual_detail

inline std::span<const MeshVertex> visualVertices() {
    return visual_detail::mesh().vertices;
}

inline void draw(const BoxRenderer& renderer, GLint visualFirst, const Room& room, const glm::vec3& eye) {
    using namespace visual_detail;
    constexpr glm::mat4 identity{1.0f};
    const auto& visual = mesh();
    const auto part = [&](Range range, const glm::mat4& model, glm::vec3 tint = glm::vec3{1.0f}) {
        renderer.drawMovableMesh(visualFirst + range.first,range.count,model,tint);
    };
    part(visual.hall,identity);
    const auto hinge = glm::translate(identity,{hallCenterX-exitHalfWidth,exitHeight,backZ-.15f});
    part(visual.door,glm::rotate(hinge, -1.40f * room.doorProgress(),{0,1,0}));
    const auto held = room.heldIndex();
    for (std::size_t i = 0; i < room.props().size(); ++i) {
        const auto& prop = room.props()[i];
        const auto placement = glm::translate(identity,prop.center);
        const auto rotation = glm::rotate(placement,.5f*std::numbers::pi_v<float>*prop.turns,{0,1,0});
        part(visual.kinds[static_cast<std::size_t>(prop.kind)],glm::scale(rotation,prop.baseSize*prop.scale),
            held == i && !room.placementReady() ? glm::vec3{.73f,.78f,.78f} : glm::vec3{1.0f});
    }
    (void)eye;
}
} // namespace home_finale
