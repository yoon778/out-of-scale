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
#include <sstream>
#include <string>

namespace {
constexpr float pi = 3.14159265358979323846f;
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
} // namespace

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
    coloredCube_=cube_;
    for (auto& v : coloredCube_) {
        v.position *= .60f;
        v.color={v.position.x>0?1.0f:.2f,v.position.y>0?1.0f:.2f,v.position.z>0?1.0f:.2f};
    }
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
    if (lesson<0 || lesson>=static_cast<int>(lessonOffsets_.size())) return;
    lesson_=lesson;
    clearInput();
}
int CourseworkLabs::selected() const { return lesson_; }
void CourseworkLabs::clearInput() { arrows_.fill(false); }

void CourseworkLabs::key(int key, int action, int) {
    int index=-1;
    switch (key) {
        case GLFW_KEY_LEFT: index=0; break;
        case GLFW_KEY_RIGHT: index=1; break;
        case GLFW_KEY_DOWN: index=2; break;
        case GLFW_KEY_UP: index=3; break;
        default: return;
    }
    if (action==GLFW_RELEASE) arrows_[index]=false;
    else if (action==GLFW_PRESS || action==GLFW_REPEAT) arrows_[index]=true;
}

bool CourseworkLabs::update(float elapsed) {
    const float dt=std::clamp(elapsed,0.0f,.10f);
    const glm::vec2 movement{float(arrows_[1])-float(arrows_[0]),float(arrows_[3])-float(arrows_[2])};
    if (dt<=0 || movement==glm::vec2{0}) return false;
    auto& offset=lessonOffsets_[lesson_];
    const auto previous=offset;
    // Keep the tall original practice-12 frames within the canvas too.
    offset=glm::clamp(offset+movement*.50f*dt,glm::vec2{-.15f},glm::vec2{.15f});
    return offset!=previous;
}

void CourseworkLabs::render(std::span<const Vertex> vertices, GLenum mode, const glm::mat4& object,
                            const glm::mat4& vp, bool lit) const {
    glUseProgram(program_); glBindVertexArray(vertexArray_); glBindBuffer(GL_ARRAY_BUFFER,vertexBuffer_);
    glUniformMatrix4fv(glGetUniformLocation(program_,"modelTransform"),1,GL_FALSE,glm::value_ptr(object));
    const auto shifted=glm::translate(glm::mat4{1},glm::vec3{lessonOffsets_[lesson_],0})*vp;
    glUniformMatrix4fv(glGetUniformLocation(program_,"viewProjection"),1,GL_FALSE,glm::value_ptr(shifted));
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
    constexpr std::array<Shape2d,4> shapes{{
        {0,{-.55f,.45f},{1,.72f,.25f}}, {1,{.35f,.45f},{.35f,.85f,1}},
        {2,{-.45f,-.35f},{.8f,.4f,.65f}}, {3,{.45f,-.35f},{.35f,.8f,.5f}}
    }};
    for (const auto& shape : shapes) {
        const auto color=shape.color;
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
    rect({-.6f,.6f},{.3f,.2f},{.75f,.33f,.47f});
    rect({.1f,-.6f},{.3f,.2f},{.30f,.65f,.81f});
}

void CourseworkLabs::drawTransform() const {
    glEnable(GL_DEPTH_TEST);
    std::vector<Vertex> axes;
    for (int axis=0;axis<3;++axis) {
        glm::vec3 a{0},b{0},color{0}; a[axis]=-.8f;b[axis]=.8f;color[axis]=1;
        axes.push_back({a,color});axes.push_back({b,color});
    }
    render(axes,GL_LINES,model({0,0,0},{-30,-30,0}),glm::mat4{1});
    render(coloredCube_,GL_TRIANGLES,model({0,0,0},{-30,-30,0}),glm::mat4{1});
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}
void CourseworkLabs::drawSolar() const {
    glEnable(GL_DEPTH_TEST);
    const auto view=glm::lookAt(glm::vec3{0,3.2f,6.5f},glm::vec3{0},glm::vec3{0,1,0});
    const auto projection=glm::perspective(glm::radians(45.0f),4.0f/3,.1f,50.0f);
    const glm::mat4 base{1};
    const auto sphere=[&](const glm::mat4& object,glm::vec3 color) { auto vertices=sphere_; for(auto& v:vertices)v.color=color; render(vertices,GL_TRIANGLES,object,projection*view); };
    const auto circle=[&](const glm::mat4& object,glm::vec3 color,bool xy) {
        std::vector<Vertex> path; path.reserve(160);
        for(int i=0;i<160;++i) { float a=2*pi*i/160; path.push_back({xy?glm::vec3{std::cos(a),std::sin(a),0}:glm::vec3{std::cos(a),0,std::sin(a)},color}); }
        render(path,GL_LINE_LOOP,object,projection*view);
    };
    sphere(base*model({0,0,0},{0,0,0},glm::vec3{.30f/.24f}),{1,.55f,.1f});
    constexpr float planetAngle[3]{0,120,240},moonAngle[3]{40,130,220};
    constexpr float radius[3]{.95f,1.35f,1.8f},incline[3]{0,45,-45};
    constexpr glm::vec3 colors[3]{{.15f,.65f,1},{.2f,.9f,.35f},{.8f,.3f,.95f}};
    constexpr glm::vec3 moonColors[3]{{.95f,.25f,.25f},{.95f,.95f,.3f},{.85f,.9f,.95f}};
    const auto cameraAxes=glm::mat4{glm::mat3{glm::inverse(view)}};
    for (int i=0;i<3;++i) {
        circle(base*model({0,0,0},{0,0,incline[i]},glm::vec3{radius[i]}),colors[i]*.65f,false);
        const float a=glm::radians(planetAngle[i]);
        const glm::vec3 p=glm::vec3{glm::rotate(glm::mat4{1},glm::radians(incline[i]),{0,0,1})*glm::vec4{radius[i]*std::cos(a),0,radius[i]*std::sin(a),1}};
        const auto frame=base*glm::translate(glm::mat4{1},p)*cameraAxes;
        sphere(frame*model({0,0,0},{0,0,0},glm::vec3{(.11f+.015f*i)/.24f}),colors[i]);
        const auto moonFrame=frame;
        const float rx=.3f,ry=.17f;
        circle(moonFrame*model({0,0,0},{0,0,0},{rx,ry,1}),moonColors[i]*.75f,true);
        const float m=glm::radians(moonAngle[i]);
        sphere(moonFrame*model({rx*std::cos(m),ry*std::sin(m),0},{0,0,0},glm::vec3{.045f/.24f}),moonColors[i]);
    }
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}
void CourseworkLabs::drawLighting() const {
    glEnable(GL_DEPTH_TEST);
    const glm::vec3 eye{0,1.8f,5.6f};
    const auto vp=glm::perspective(glm::radians(45.0f),4.0f/3,.1f,30.0f)*glm::lookAt(eye,glm::vec3{0},glm::vec3{0,1,0});
    constexpr float lightRadius=2;
    const float a=glm::radians(43.0f);
    const glm::vec3 light{lightRadius*std::sin(a),0,lightRadius*std::cos(a)};
    glUseProgram(program_);
    glUniform3fv(glGetUniformLocation(program_,"lightPosition"),1,glm::value_ptr(light));
    glUniform3fv(glGetUniformLocation(program_,"viewPosition"),1,glm::value_ptr(eye));
    std::vector<Vertex> path;
    for(int i=0;i<96;++i) { const float t=2*pi*i/96; path.push_back({{std::sin(t),0,std::cos(t)},{.24f,.38f,.27f}}); }
    render(path,GL_LINE_LOOP,model({0,0,0},{0,0,0},{lightRadius,1,lightRadius}),vp);
    render(cube_,GL_TRIANGLES,model({0,0,0},{0,-25,0}),vp,true);
    auto marker=cube_; for(auto& v:marker)v.color={1,.88f,.56f};
    render(marker,GL_TRIANGLES,model(light,{0,0,0},glm::vec3{.2f}),vp);
}
void CourseworkLabs::draw() const {
    constexpr glm::vec3 backgrounds[5]{{.2f,.3f,.4f},{1,1,1},{.15f,.15f,.15f},
                                     {.06f,.07f,.10f},{.035f,.04f,.05f}};
    const auto color=backgrounds[lesson_];
    glClearColor(color.r,color.g,color.b,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    switch(lesson_) {
    case 0:drawShapes();break; case 1:drawStack();break; case 2:drawTransform();break;
    case 3:drawSolar();break; case 4:drawLighting();break;
    }
}
