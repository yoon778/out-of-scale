#pragma once

#include "SchoolRoom.h"
#include <string>

struct GLFWwindow;

namespace school {
struct PreviewState {
    SchoolRoom room;
    Camera camera;
};

struct PreviewPause {
    void* context;
    bool (*isPaused)(const void*);
    void (*setPaused)(void*, bool);
    bool (*keyPressed)(void*, GLFWwindow*, int);
    void (*click)(void*, GLFWwindow*);
    void (*draw)(void*, GLFWwindow*, int, int);
};

struct PreviewNavigation {
    int requestedKey = 0;
    bool (*restoreFullscreen)(GLFWwindow*) = nullptr;
    const PreviewPause* pause = nullptr;
    void* frameContext = nullptr;
    void (*afterUpdate)(void*, GLFWwindow*) = nullptr;
};

int runPreview(GLFWwindow* window, PreviewState& preview, const std::string& captureFolder = {},
               PreviewNavigation* navigation = nullptr);
} // namespace school
