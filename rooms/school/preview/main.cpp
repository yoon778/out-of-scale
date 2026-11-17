#include "../SchoolPreview.h"
#include "../SchoolRenderer.h"
#include <GL/glfw3.h>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string captures;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--self-test")
            return school::runTests();
        if (std::string(argv[i]) == "--capture-dir" && i + 1 < argc)
            captures = argv[++i];
    }
    glfwSetErrorCallback([](int, const char* text) { std::cerr << text << '\n'; });
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    if (!captures.empty())
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window =
        glfwCreateWindow(1280, 800, "Out of Scale | School - Chalk Outside the Blackboard", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    while (glGetError() != GL_NO_ERROR) {
    } // GLEW may query a legacy enum during core-context initialization.
    glfwSwapInterval(captures.empty() ? 1 : 0);
    int result = 0;
    try {
        school::PreviewState state;
        result = school::runPreview(window, state, captures);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 5;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
