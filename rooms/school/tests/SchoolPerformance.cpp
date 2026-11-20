#include "../SchoolPreview.cpp"
#include <chrono>
#include <iomanip>
#include <numeric>
using Clock = std::chrono::steady_clock;
double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b-a).count(); }
void stats(const char* name, std::vector<double> v) {
    double mean = std::accumulate(v.begin(), v.end(), 0.) / v.size();
    std::sort(v.begin(), v.end());
    std::cout << ' ' << name << '=' << mean << '/' << v[size_t(v.size()*.95)] << '/' << v[size_t(v.size()*.99)];
}
int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* w = glfwCreateWindow(1280, 800, "Hidden school profile", nullptr, nullptr);
    if (!w) return 2;
    glfwMakeContextCurrent(w); glewExperimental = GL_TRUE; glewInit();
    while(glGetError()!=GL_NO_ERROR) {}
    glfwSwapInterval(0);
    std::cout << glGetString(GL_RENDERER) << " 1280x800 MSAA4 vsync0 mean/p95/p99 ms\n" << std::fixed << std::setprecision(3);
    {
        school::Renderer r; if(!r.initialize()) return 3;
        GLuint query; glGenQueries(1, &query);
        for (const std::string scenario : {"walk", "mirror-look", "mirror-turn"}) {
            for(int run=0;run<3;++run) {
                school::PreviewState p; Application state{p.room,p.camera};
                settle(state);
                if(scenario!="walk") stageMirrorCapture(state,"mirror-aligned");
                if(scenario=="mirror-turn") { state.camera.face(school::SchoolRoom::mirrorAim); p.room.toggleMirror(p.camera); }
                const float yaw=p.camera.yaw, pitch=p.camera.pitch;
                std::vector<double> cpu, update, total, gpu;
                for(int f=-120;f<600;++f) {
                    float phase=float(f+120)*.015f;
                    p.camera.yaw=yaw+std::sin(phase)*.45f; p.camera.pitch=pitch+.08f*std::sin(phase*.7f);
                    school::Input input{};
                    if(scenario=="walk") input.right=(f/120%2==0 ? .4f : -.4f);
                    if(scenario=="mirror-turn") p.room.rotateMirror(std::cos(phase)*.003f);
                    auto a=Clock::now(); p.room.update(p.camera,input,1.f/120); auto b=Clock::now();
                    glBeginQuery(GL_TIME_ELAPSED,query); p.room.draw(r,p.camera,1280,800); glEndQuery(GL_TIME_ELAPSED);
                    auto c=Clock::now(); glFinish(); auto d=Clock::now();
                    GLuint64 ns; glGetQueryObjectui64v(query,GL_QUERY_RESULT,&ns);
                    if(f>=0) {update.push_back(ms(a,b));cpu.push_back(ms(b,c));total.push_back(ms(a,d));gpu.push_back(ns/1e6);}
                }
                std::cout<<scenario<<" run="<<run+1; stats("update",update);stats("submit",cpu);stats("gpu",gpu);stats("total",total);std::cout<<std::endl;
            }
        }
        glDeleteQueries(1,&query);
        if(glGetError()!=GL_NO_ERROR) return 4;
    }
    glfwDestroyWindow(w);glfwTerminate();return 0;
}
