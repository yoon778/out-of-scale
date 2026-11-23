#include "CourseworkLabs.h"

#include <windows.h>
#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace {
constexpr float pi = 3.14159265358979323846f;
constexpr std::array<glm::vec2,4> keyOffsets{{{0,0},{.61f,.07f},{.77f,-.07f},{.99f,-.07f}}};
glm::mat4 model(glm::vec3 position, glm::vec3 angle, glm::vec3 size = glm::vec3{1}) {
    auto value = glm::translate(glm::mat4{1}, position);
    value = glm::rotate(value, glm::radians(angle.x), {1,0,0});
    value = glm::rotate(value, glm::radians(angle.y), {0,1,0});
    value = glm::rotate(value, glm::radians(angle.z), {0,0,1});
    return glm::scale(value, size);
}
GLuint compile(GLenum kind, const char* source) {
    const GLuint result = glCreateShader(kind);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint valid = GL_FALSE;
    glGetShaderiv(result, GL_COMPILE_STATUS, &valid);
    if (valid == GL_TRUE) return result;
    glDeleteShader(result);
    return 0;
}
std::filesystem::path spherePath() {
    wchar_t executable[32768]{};
    GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    const std::array roots{std::filesystem::current_path(), std::filesystem::path(executable).parent_path(),
                           std::filesystem::path(__FILE__).parent_path()};
    for (auto root : roots) {
        for (int level = 0; level < 8 && !root.empty(); ++level, root = root.parent_path()) {
            for (const auto& relative : {std::filesystem::path(L"rooms/home/coursework/sphere.obj"),
                                        std::filesystem::path(L"coursework/sphere.obj")}) {
                const auto candidate = root / relative;
                if (std::filesystem::is_regular_file(candidate)) return candidate;
            }
            if (root == root.parent_path()) break;
        }
    }
    return {};
}
glm::vec2 rotate2(glm::vec2 value, float angle) {
    return {value.x*std::cos(angle)-value.y*std::sin(angle),
            value.x*std::sin(angle)+value.y*std::cos(angle)};
}
} // namespace

CourseworkLabs::CourseworkLabs() : keyParts_{{
    {{-.65f,.45f},{.40f,.40f},0,1},
    {{.20f,-.45f},{.86f,.14f},.35f,.85f},
    {{.55f,.45f},{.09f,.14f},-.35f,1.20f},
    {{-.55f,-.45f},{.09f,.14f},.35f,.85f}
}} {}

CourseworkLabs::~CourseworkLabs() {
    if (program_) glDeleteProgram(program_);
    if (vertexArray_) glDeleteVertexArrays(1, &vertexArray_);
    if (vertexBuffer_) glDeleteBuffers(1, &vertexBuffer_);
}

bool CourseworkLabs::initialize() {
    // Practice 26's normal transform and Phong equation, also used unlit by 7/12/14/18.
    constexpr char vertex[] = R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=2) in vec3 normal;
uniform mat4 modelTransform;
uniform mat4 viewProjection;
out vec3 worldPosition;
out vec3 worldNormal;
out vec3 vertexColor;
void main() {
    vec4 world=modelTransform*vec4(position,1);
    worldPosition=world.xyz;
    worldNormal=transpose(inverse(mat3(modelTransform)))*normal;
    vertexColor=color;
    gl_Position=viewProjection*world;
})";
    constexpr char fragment[] = R"(#version 330 core
in vec3 worldPosition;
in vec3 worldNormal;
in vec3 vertexColor;
uniform vec3 lightPosition;
uniform vec3 viewPosition;
uniform bool lightingEnabled;
out vec4 pixel;
void main() {
    if(!lightingEnabled) { pixel=vec4(vertexColor,1); return; }
    vec3 normal=normalize(worldNormal);
    vec3 toLight=normalize(lightPosition-worldPosition);
    vec3 toView=normalize(viewPosition-worldPosition);
    vec3 reflected=reflect(-toLight,normal);
    float diffuse=max(dot(normal,toLight),0);
    float specular=diffuse>0?pow(max(dot(reflected,toView),0),24):0;
    pixel=vec4(.15*vertexColor+.8*diffuse*vertexColor+.25*specular,1);
})";
    const GLuint vs = compile(GL_VERTEX_SHADER, vertex), fs = compile(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs); glAttachShader(program_, fs); glLinkProgram(program_);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) return false;
    glGenVertexArrays(1, &vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,position)));
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,color)));
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,normal)));
    for (GLuint index=0; index<3; ++index) glEnableVertexAttribArray(index);

    const auto triangle = [](std::vector<Vertex>& target, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        const auto normal = glm::normalize(glm::cross(b-a,c-a));
        for (const auto p : {a,b,c}) target.push_back({p,{.10f,.95f,.18f},normal});
    };
    constexpr glm::vec3 p[8]{{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
                            {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}};
    constexpr int faces[6][6]{{4,5,6,4,6,7},{1,0,3,1,3,2},{0,4,7,0,7,3},
                             {5,1,2,5,2,6},{7,6,2,7,2,3},{0,1,5,0,5,4}};
    for (const auto& face : faces) for (int i=0;i<6;i+=3)
        triangle(cube_,p[face[i]],p[face[i+1]],p[face[i+2]]);
    const glm::vec3 apex{0,.5f,0};
    for (const auto pair : {glm::ivec2{4,5},glm::ivec2{5,1},glm::ivec2{1,0},glm::ivec2{0,4}})
        triangle(pyramid_,p[pair.x],p[pair.y],apex);
    triangle(pyramid_,p[0],p[1],p[5]); triangle(pyramid_,p[0],p[5],p[4]);
    const auto colored = [](const std::vector<Vertex>& input, bool pyramid) {
        auto result = input;
        for (auto& v : result) {
            v.position *= .60f;
            if (pyramid && v.position.y>0) { v.position.y=.4f; v.color={1,1,1}; }
            else v.color={v.position.x>0?1.0f:.2f,v.position.y>0?1.0f:.2f,v.position.z>0?1.0f:.2f};
        }
        return result;
    };
    coloredCube_=colored(cube_,false); coloredPyramid_=colored(pyramid_,true);
    std::ifstream file(spherePath());
    if (!file) return false;
    std::vector<glm::vec3> positions;
    std::string line;
    while (std::getline(file,line)) {
        std::istringstream input(line); std::string kind; input>>kind;
        if (kind=="v") { glm::vec3 value; input>>value.x>>value.y>>value.z; positions.push_back(value); }
        else if (kind=="f") {
            std::vector<int> face; std::string word;
            while (input>>word) {
                const int index=std::stoi(word.substr(0,word.find('/')));
                face.push_back(index>0?index-1:static_cast<int>(positions.size())+index);
            }
            for (std::size_t i=1;i+1<face.size();++i) for (const int index : {face[0],face[i],face[i+1]}) {
                if (index<0 || index>=static_cast<int>(positions.size())) return false;
                sphere_.push_back({positions[index],{1,1,1}});
            }
        }
    }
    return !sphere_.empty();
}

void CourseworkLabs::select(int lesson) {
    if (lesson<0 || lesson>5) return;
    lesson_=lesson; clearInput();
}
int CourseworkLabs::selected() const { return lesson_; }
void CourseworkLabs::clearInput() { pressed_.fill(false); dragging_=false; }

void CourseworkLabs::createShape(int type) {
    if (shapes_.size()>=50) return;
    const auto randomValue=[&] { random_=1664525u*random_+1013904223u; return float(random_>>8)/16777215.0f; };
    shapes_.push_back({type,{randomValue()*1.5f-.75f,randomValue()*1.5f-.75f},
                       {.2f+.8f*randomValue(),.2f+.8f*randomValue(),.2f+.8f*randomValue()}});
}
void CourseworkLabs::moveShape(int index, glm::vec2 delta) {
    if (index<0 || index>=static_cast<int>(shapes_.size())) return;
    auto& shape=shapes_[index]; const float half=shape.type==0?.025f:.12f;
    const auto next=shape.center+delta;
    if (std::abs(next.x)+half<=1 && std::abs(next.y)+(shape.type==1?0:half)<=1) shape.center=next;
}
void CourseworkLabs::resetStack() {
    stackY_={.5f,-.7f}; stackDirection_={-1,1}; stackCount_=0; stackTravel_=-1;
}
void CourseworkLabs::resetTransform() { transformMove_={0,0}; transformAngle_={-30,-30}; transformDirection_={0,0}; }

void CourseworkLabs::key(int key, int action, int mods) {
    if (key>=0 && key<static_cast<int>(pressed_.size())) pressed_[key]=action!=GLFW_RELEASE;
    if (action==GLFW_RELEASE) return;
    const bool first=action==GLFW_PRESS, shift=(mods&GLFW_MOD_SHIFT)!=0;
    if (lesson_==0) {
        if (first) {
            if (key==GLFW_KEY_P) createShape(0);
            if (key==GLFW_KEY_E) createShape(1);
            if (key==GLFW_KEY_T) createShape(2);
            if (key==GLFW_KEY_R) createShape(3);
            if (key==GLFW_KEY_C) { shapes_.clear(); shapeSelection_=-1; }
        }
        glm::vec2 delta{0};
        if (key==GLFW_KEY_W) delta.y=.05f;
        if (key==GLFW_KEY_S) delta.y=-.05f;
        if (key==GLFW_KEY_A) delta.x=-.05f;
        if (key==GLFW_KEY_D) delta.x=.05f;
        if (key==GLFW_KEY_I) delta={-.05f,.05f};
        if (key==GLFW_KEY_J) delta={.05f,.05f};
        if (key==GLFW_KEY_K) delta={-.05f,-.05f};
        if (key==GLFW_KEY_L) delta={.05f,-.05f};
        moveShape(shapeSelection_,delta);
        if (key>=GLFW_KEY_1 && key<=GLFW_KEY_4) {
            constexpr glm::vec2 offsets[4]{{-.05f,0},{.05f,0},{0,.05f},{0,-.05f}};
            for (int i=0;i<static_cast<int>(shapes_.size());++i) moveShape(i,offsets[key-GLFW_KEY_1]);
        }
    } else if (lesson_==1 && first) {
        if (key==GLFW_KEY_R) resetStack();
        if ((key==GLFW_KEY_ENTER || key==GLFW_KEY_KP_ENTER) && stackTravel_<0 &&
            stackY_[0]>=-.2f && stackY_[0]+.2f<=.2f && stackY_[1]>=-.2f && stackY_[1]+.2f<=.2f) {
            stackTravel_=0; stackMatchY_=(stackY_[0]+stackY_[1])*.5f;
        }
    } else if (lesson_==2 && first) {
        if (key==GLFW_KEY_C) transformCube_=true;
        if (key==GLFW_KEY_P) transformCube_=false;
        if (key==GLFW_KEY_H) transformDepth_=!transformDepth_;
        if (key==GLFW_KEY_W) transformWire_=!shift;
        if (key==GLFW_KEY_X) transformDirection_.x=shift?-1.0f:1.0f;
        if (key==GLFW_KEY_Y) transformDirection_.y=shift?-1.0f:1.0f;
        if (key==GLFW_KEY_S) resetTransform();
    } else if (lesson_==3 && first) {
        if (key==GLFW_KEY_P) solarPerspective_=shift;
        if (key==GLFW_KEY_M) solarWire_=shift;
        if (key==GLFW_KEY_1) solarLarge_=!solarLarge_;
        if (key==GLFW_KEY_2) solarPulse_=!solarPulse_;
        if (key==GLFW_KEY_3) solarMoon_=!solarMoon_;
    } else if (lesson_==4) {
        if (first) {
            if (key==GLFW_KEY_N) lightingCube_=!lightingCube_;
            if (key==GLFW_KEY_M) lightingEnabled_=!lightingEnabled_;
            if (key==GLFW_KEY_Y) lightingSpin_=!lightingSpin_;
            if (key==GLFW_KEY_R) { const int direction=shift?-1:1; lightOrbit_=lightOrbit_==direction?0:direction; }
        }
        if (key==GLFW_KEY_Z) lightRadius_=std::clamp(lightRadius_+(shift?.15f:-.15f),1.15f,3.3f);
    }
}

void CourseworkLabs::pointer(glm::vec2 ndc, bool inside) {
    pointer_=ndc; pointerInside_=inside;
    if (lesson_==5 && dragging_ && partSelection_>=0 && !keyTaken_) {
        keyParts_[partSelection_].center=pointer_+dragOffset_;
        clampPart(keyParts_[partSelection_]);
    }
}
void CourseworkLabs::mouse(int button, int action) {
    if (button!=GLFW_MOUSE_BUTTON_LEFT) return;
    if (action==GLFW_RELEASE) {
        if (lesson_==5 && dragging_ && pointerInside_ && !keyTaken_) snapKeyPart(partSelection_);
        dragging_=false;
        return;
    }
    if (action!=GLFW_PRESS || !pointerInside_) return;
    if (lesson_==0) {
        shapeSelection_=-1;
        for (int i=static_cast<int>(shapes_.size())-1;i>=0;--i) {
            const auto& shape=shapes_[i]; const auto d=pointer_-shape.center;
            const float half=shape.type==0?.025f:.12f;
            bool hit=std::abs(d.x)<=half && std::abs(d.y)<=(shape.type==1?.03f:half);
            if (shape.type==2) hit=hit && std::abs(d.x)<=(half-d.y)*.5f;
            if (hit) { shapeSelection_=i; break; }
        }
    } else if (lesson_==5 && !keyTaken_) {
        partSelection_=-1;
        const auto hit = [&](int i,float padding) {
            const auto& part=keyParts_[i];
            const auto local=rotate2(pointer_-part.center,-part.angle);
            const auto half=part.size*part.scale*.5f+glm::vec2{padding};
            return std::abs(local.x)<=half.x && std::abs(local.y)<=half.y;
        };
        // Pick visible parts first; expanded tooth hit areas must not steal shaft clicks.
        for (int i=3;i>=0;--i) {
            if (hit(i,.012f)) { partSelection_=i; break; }
        }
        if (partSelection_<0) {
            float nearest=std::numeric_limits<float>::max();
            for (int i=2;i<4;++i) {
                const float distance=glm::length(pointer_-keyParts_[i].center);
                if (hit(i,.04f) && distance<nearest) { nearest=distance; partSelection_=i; }
            }
        }
        if (partSelection_>=0) {
            dragging_=true;
            dragOffset_=keyParts_[partSelection_].center-pointer_;
        }
    }
}
void CourseworkLabs::clampPart(Part& part) {
    const auto half=part.size*part.scale*.5f;
    const glm::vec2 extent{std::abs(std::cos(part.angle))*half.x+std::abs(std::sin(part.angle))*half.y,
                           std::abs(std::sin(part.angle))*half.x+std::abs(std::cos(part.angle))*half.y};
    part.center=glm::clamp(part.center,-glm::vec2{.97f}+extent,glm::vec2{.97f}-extent);
}
void CourseworkLabs::snapKeyPart(int index) {
    if (index<=0) return;
    const auto& head=keyParts_[0];
    auto& part=keyParts_[index];
    if (std::abs(part.scale/head.scale-1)>.35f) return;
    float nearest=.18f;
    Part best=part;
    const int firstSlot=index==1?1:2;
    const int lastSlot=index==1?1:3;
    for (int quarter=0;quarter<4;++quarter) {
        const float axis=head.angle+quarter*pi*.5f;
        if (std::abs(std::remainder(part.angle-axis,pi))>.60f) continue;
        for (int slot=firstSlot;slot<=lastSlot;++slot) {
            const auto target=head.center+rotate2(keyOffsets[slot]*head.scale,axis);
            const float distance=glm::length(part.center-target)/head.scale;
            if (distance>nearest) continue;
            if (index>=2) {
                const auto& other=keyParts_[5-index];
                if (glm::length(other.center-target)/head.scale<.055f &&
                    std::abs(other.scale/head.scale-1)<.10f &&
                    std::abs(std::remainder(other.angle-axis,pi))<.12f) continue;
            }
            Part candidate=part;
            candidate.center=target; candidate.angle=axis; candidate.scale=head.scale;
            clampPart(candidate);
            if (glm::length(candidate.center-target)>.001f) continue;
            nearest=distance;
            best=candidate;
        }
    }
    part=best;
}
void CourseworkLabs::scroll(double amount) {
    if (lesson_!=5 || partSelection_<0 || keyTaken_) return;
    auto& part=keyParts_[partSelection_];
    part.scale=std::clamp(part.scale*std::exp(-static_cast<float>(amount)*.12f),.60f,1.40f);
    clampPart(part);
}
bool CourseworkLabs::update(float elapsed) {
    const float dt=std::clamp(elapsed,0.0f,.10f);
    if (lesson_==1) {
        if (stackTravel_>=0) {
            stackTravel_+=dt/1.1f;
            if (stackTravel_>=1) { stackCount_+=2; stackTravel_=-1; stackY_={.5f,-.7f}; stackDirection_={-1,1}; }
        } else for (int i=0;i<2;++i) {
            stackY_[i]+=.3f*dt*stackDirection_[i];
            if (stackY_[i]<-.8f) { stackY_[i]=-.8f; stackDirection_[i]=1; }
            if (stackY_[i]>.6f) { stackY_[i]=.6f; stackDirection_[i]=-1; }
        }
        return true;
    }
    const auto down=[&](int key) { return pressed_[key]?1.0f:0.0f; };
    if (lesson_==2) {
        transformAngle_+=transformDirection_*60.0f*dt;
        transformMove_+=glm::vec2{down(GLFW_KEY_RIGHT)-down(GLFW_KEY_LEFT),down(GLFW_KEY_UP)-down(GLFW_KEY_DOWN)}*.3f*dt;
        return glm::length(transformDirection_)>0 || down(GLFW_KEY_RIGHT)+down(GLFW_KEY_LEFT)+down(GLFW_KEY_UP)+down(GLFW_KEY_DOWN)>0;
    }
    if (lesson_==3) {
        solarMove_+=glm::vec3{down(GLFW_KEY_D)-down(GLFW_KEY_A),down(GLFW_KEY_W)-down(GLFW_KEY_S),
                             down(GLFW_KEY_EQUAL)+down(GLFW_KEY_KP_ADD)-down(GLFW_KEY_MINUS)-down(GLFW_KEY_KP_SUBTRACT)}*.8f*dt;
        const float target=solarLarge_?1.35f:1;
        orbitScale_+=std::clamp(target-orbitScale_,-.25f*dt,.25f*dt);
        if (solarPulse_) solarTime_+=dt;
        constexpr float speeds[3]{24,38,-29};
        for (int i=0;i<3;++i) { planetAngle_[i]=std::fmod(planetAngle_[i]+speeds[i]*dt,360.0f); moonAngle_[i]=std::fmod(moonAngle_[i]+(85+20*i)*dt,360.0f); }
        if (solarMoon_) moonRotation_=std::fmod(moonRotation_+40*dt,360.0f);
        return true;
    }
    if (lesson_==4) {
        if (lightingSpin_) objectAngle_=std::fmod(objectAngle_+60*dt,360.0f);
        lightAngle_=std::fmod(lightAngle_+lightOrbit_*65*dt,360.0f);
        return lightingSpin_ || lightOrbit_!=0;
    }
    if (lesson_==5 && partSelection_>=0 && !keyTaken_) {
        const float turn=down(GLFW_KEY_R)-down(GLFW_KEY_Q);
        if (turn!=0) { auto& part=keyParts_[partSelection_]; part.angle=std::remainder(part.angle+turn*1.6f*dt,2*pi); clampPart(part); return true; }
    }
    return false;
}

bool CourseworkLabs::keyReady() const {
    if (keyTaken_ || dragging_) return false;
    const auto& head=keyParts_[0];
    // Rectangles repeat after 180 degrees; the square head repeats after 90.
    // Infer the key's axis from the shaft, rather than an invisible square orientation.
    for (const float axis : {keyParts_[1].angle,keyParts_[1].angle+pi}) {
        if (std::abs(std::remainder(head.angle-axis,pi*.5f))>.12f) continue;
        const auto matches = [&](int index,int slot) {
            const auto& part=keyParts_[index];
            const auto local=rotate2(part.center-head.center,-axis)/head.scale;
            return glm::length(local-keyOffsets[slot])<=.055f && std::abs(part.scale/head.scale-1)<=.10f &&
                   std::abs(std::remainder(part.angle-axis,pi))<=.12f;
        };
        if (matches(1,1) && ((matches(2,2) && matches(3,3)) ||
                             (matches(2,3) && matches(3,2)))) return true;
    }
    return false;
}
bool CourseworkLabs::keyTaken() const { return keyTaken_; }
void CourseworkLabs::consumeKey() { keyTaken_=true; dragging_=false; partSelection_=-1; }

void CourseworkLabs::render(std::span<const Vertex> vertices, GLenum mode, const glm::mat4& object,
                            const glm::mat4& vp, bool lit) const {
    glUseProgram(program_); glBindVertexArray(vertexArray_); glBindBuffer(GL_ARRAY_BUFFER,vertexBuffer_);
    glUniformMatrix4fv(glGetUniformLocation(program_,"modelTransform"),1,GL_FALSE,glm::value_ptr(object));
    glUniformMatrix4fv(glGetUniformLocation(program_,"viewProjection"),1,GL_FALSE,glm::value_ptr(vp));
    glUniform1i(glGetUniformLocation(program_,"lightingEnabled"),lit);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size_bytes()),vertices.data(),GL_STREAM_DRAW);
    glDrawArrays(mode,0,static_cast<GLsizei>(vertices.size()));
}
void CourseworkLabs::rect(glm::vec2 center, glm::vec2 size, glm::vec3 color,
                          const glm::mat4& object, bool line) const {
    const auto half=size*.5f;
    const std::array<Vertex,6> v{{{{center.x-half.x,center.y-half.y,0},color},{{center.x+half.x,center.y-half.y,0},color},
                                 {{center.x+half.x,center.y+half.y,0},color},{{center.x-half.x,center.y-half.y,0},color},
                                 {{center.x+half.x,center.y+half.y,0},color},{{center.x-half.x,center.y+half.y,0},color}}};
    if (line) { const std::array<Vertex,4> outline{{v[0],v[1],v[2],v[5]}}; render(outline,GL_LINE_LOOP,object,glm::mat4{1}); }
    else render(v,GL_TRIANGLES,object,glm::mat4{1});
}
void CourseworkLabs::drawShapes() const {
    glDisable(GL_DEPTH_TEST);
    for (int i=0;i<static_cast<int>(shapes_.size());++i) {
        const auto& shape=shapes_[i]; const auto color=i==shapeSelection_?glm::vec3{1,1,0}:shape.color;
        if (shape.type==0 || shape.type==3) rect(shape.center,glm::vec2{shape.type==0?.05f:.24f},color);
        else if (shape.type==1) {
            const std::array<Vertex,2> line{{{{shape.center.x-.12f,shape.center.y,0},color},{{shape.center.x+.12f,shape.center.y,0},color}}};
            render(line,GL_LINES,glm::mat4{1},glm::mat4{1});
        } else {
            const std::array<Vertex,3> triangle{{{{shape.center.x-.12f,shape.center.y-.12f,0},color},
                                                 {{shape.center.x+.12f,shape.center.y-.12f,0},color},{{shape.center.x,shape.center.y+.12f,0},color}}};
            render(triangle,GL_TRIANGLES,glm::mat4{1},glm::mat4{1});
        }
    }
}
void CourseworkLabs::drawStack() const {
    glDisable(GL_DEPTH_TEST);
    rect({-.6f,0},{.4f,1.6f},{0,0,0},glm::mat4{1},true);
    rect({.1f,0},{.4f,1.6f},{0,0,0},glm::mat4{1},true);
    std::vector<Vertex> dash;
    const auto segment=[&](glm::vec2 a,glm::vec2 b) { dash.push_back({{a,0},{.2f,.5f,.8f}}); dash.push_back({{b,0},{.2f,.5f,.8f}}); };
    for (float x=-.9f;x<.4f;x+=.1f) for (float y : {-.2f,.2f}) segment({x,y},{std::min(x+.05f,.4f),y});
    for (float y=-.2f;y<.2f;y+=.1f) for (float x : {-.9f,.4f}) segment({x,y},{x,std::min(y+.05f,.2f)});
    render(dash,GL_LINES,glm::mat4{1},glm::mat4{1});
    const int total=stackCount_+(stackTravel_>=0?2:0);
    const float h=std::min(.2f,1.6f/std::max(1,total));
    for (int i=0;i<stackCount_;++i) rect({.7f,-.8f+h*(i+.5f)},{.3f,h},{.25f+.12f*(i%4),.60f,.68f-.09f*(i%3)});
    for (int i=0;i<2;++i) {
        const glm::vec2 start{i==0?-.6f:.1f,(stackTravel_<0?stackY_[i]:stackMatchY_)+.1f};
        const glm::vec2 target{.7f,-.8f+h*(stackCount_+i+.5f)};
        const auto center=stackTravel_<0?start:glm::mix(start,target,std::min(stackTravel_,1.0f));
        rect(center,{.3f,stackTravel_<0?.2f:h},i==0?glm::vec3{.75f,.33f,.47f}:glm::vec3{.30f,.65f,.81f});
    }
}
void CourseworkLabs::drawTransform() const {
    if (transformDepth_) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    std::vector<Vertex> axes;
    for (int axis=0;axis<3;++axis) {
        glm::vec3 a{0},b{0},color{0}; a[axis]=-.8f;b[axis]=.8f;color[axis]=1;
        axes.push_back({a,color});axes.push_back({b,color});
    }
    render(axes,GL_LINES,model({0,0,0},{-30,-30,0}),glm::mat4{1});
    glPolygonMode(GL_FRONT_AND_BACK,transformWire_?GL_LINE:GL_FILL);
    render(transformCube_?coloredCube_:coloredPyramid_,GL_TRIANGLES,
           model({transformMove_,0},{transformAngle_.x,transformAngle_.y,0}),glm::mat4{1});
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}
void CourseworkLabs::drawSolar() const {
    glEnable(GL_DEPTH_TEST); glPolygonMode(GL_FRONT_AND_BACK,solarWire_?GL_LINE:GL_FILL);
    const auto view=glm::lookAt(glm::vec3{0,3.2f,6.5f},glm::vec3{0},glm::vec3{0,1,0});
    const auto projection=solarPerspective_?glm::perspective(glm::radians(45.0f),4.0f/3,.1f,50.0f):glm::ortho(-4.0f,4.0f,-3.0f,3.0f,.1f,50.0f);
    const auto base=glm::translate(glm::mat4{1},solarMove_);
    const auto sphere=[&](const glm::mat4& object,glm::vec3 color) { auto vertices=sphere_; for(auto& v:vertices)v.color=color; render(vertices,GL_TRIANGLES,object,projection*view); };
    const auto circle=[&](const glm::mat4& object,glm::vec3 color,bool xy) {
        std::vector<Vertex> path; path.reserve(160);
        for(int i=0;i<160;++i) { float a=2*pi*i/160; path.push_back({xy?glm::vec3{std::cos(a),std::sin(a),0}:glm::vec3{std::cos(a),0,std::sin(a)},color}); }
        render(path,GL_LINE_LOOP,object,projection*view);
    };
    const float pulse=1+.25f*std::sin(solarTime_*2);
    const glm::vec3 centerColor{1,.5f+.2f*std::sin(solarTime_*1.5f),.08f+.06f*std::cos(solarTime_*1.5f)};
    sphere(base*model({0,0,0},{0,0,0},glm::vec3{.30f*pulse/.24f}),solarPulse_||solarTime_>0?centerColor:glm::vec3{1,.55f,.1f});
    constexpr float radius[3]{.95f,1.35f,1.8f},incline[3]{0,45,-45};
    constexpr glm::vec3 colors[3]{{.15f,.65f,1},{.2f,.9f,.35f},{.8f,.3f,.95f}};
    constexpr glm::vec3 moonColors[3]{{.95f,.25f,.25f},{.95f,.95f,.3f},{.85f,.9f,.95f}};
    const auto cameraAxes=glm::mat4{glm::mat3{glm::inverse(view)}};
    for (int i=0;i<3;++i) {
        circle(base*model({0,0,0},{0,0,incline[i]},glm::vec3{radius[i]*orbitScale_}),colors[i]*.65f,false);
        const float a=glm::radians(planetAngle_[i]);
        const glm::vec3 p=glm::vec3{glm::rotate(glm::mat4{1},glm::radians(incline[i]),{0,0,1})*glm::vec4{radius[i]*orbitScale_*std::cos(a),0,radius[i]*orbitScale_*std::sin(a),1}};
        const auto frame=base*glm::translate(glm::mat4{1},p)*cameraAxes;
        sphere(frame*model({0,0,0},{0,0,0},glm::vec3{(.11f+.015f*i)/.24f}),colors[i]);
        const auto moonFrame=glm::rotate(frame,glm::radians(moonRotation_),{0,0,1});
        const float rx=.3f*orbitScale_,ry=.17f*orbitScale_;
        circle(moonFrame*model({0,0,0},{0,0,0},{rx,ry,1}),moonColors[i]*.75f,true);
        const float m=glm::radians(moonAngle_[i]);
        sphere(moonFrame*model({rx*std::cos(m),ry*std::sin(m),0},{0,0,0},glm::vec3{.045f/.24f}),moonColors[i]);
    }
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}
void CourseworkLabs::drawLighting() const {
    glEnable(GL_DEPTH_TEST);
    const glm::vec3 eye{0,1.8f,5.6f};
    const auto vp=glm::perspective(glm::radians(45.0f),4.0f/3,.1f,30.0f)*glm::lookAt(eye,glm::vec3{0},glm::vec3{0,1,0});
    const float a=glm::radians(lightAngle_);
    const glm::vec3 light{lightRadius_*std::sin(a),0,lightRadius_*std::cos(a)};
    glUseProgram(program_);
    glUniform3fv(glGetUniformLocation(program_,"lightPosition"),1,glm::value_ptr(light));
    glUniform3fv(glGetUniformLocation(program_,"viewPosition"),1,glm::value_ptr(eye));
    std::vector<Vertex> path;
    for(int i=0;i<96;++i) { const float t=2*pi*i/96; path.push_back({{std::sin(t),0,std::cos(t)},{.24f,.38f,.27f}}); }
    render(path,GL_LINE_LOOP,model({0,0,0},{0,0,0},{lightRadius_,1,lightRadius_}),vp);
    render(lightingCube_?cube_:pyramid_,GL_TRIANGLES,model({0,0,0},{0,objectAngle_,0}),vp,lightingEnabled_);
    auto marker=cube_; for(auto& v:marker)v.color={1,.88f,.56f};
    render(marker,GL_TRIANGLES,model(light,{0,0,0},glm::vec3{.2f}),vp);
}
void CourseworkLabs::drawAssembly() const {
    glDisable(GL_DEPTH_TEST);
    if (keyTaken_) return;
    for(int i=0;i<4;++i) {
        const auto& part=keyParts_[i];
        const auto transform=model({part.center,0},{0,0,glm::degrees(part.angle)},glm::vec3{part.scale,part.scale,1});
        rect({0,0},part.size,i==0?glm::vec3{.34f,.74f,.85f}:glm::vec3{.88f,.66f,.29f},transform);
    }
}
void CourseworkLabs::draw() const {
    constexpr glm::vec3 backgrounds[6]{{.2f,.3f,.4f},{1,1,1},{.15f,.15f,.15f},
                                     {.06f,.07f,.10f},{.035f,.04f,.05f},{.055f,.085f,.11f}};
    const auto color=backgrounds[lesson_];
    glClearColor(color.r,color.g,color.b,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    switch(lesson_) {
    case 0:drawShapes();break; case 1:drawStack();break; case 2:drawTransform();break;
    case 3:drawSolar();break; case 4:drawLighting();break; case 5:drawAssembly();break;
    }
}
