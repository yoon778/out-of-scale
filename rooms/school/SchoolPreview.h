#pragma once

#include "SchoolRoom.h"
#include <string>

struct GLFWwindow;

namespace school {
struct PreviewState {
    SchoolRoom room;
    Camera camera;
};

struct PreviewNavigation {
    int requestedKey = 0;
    bool (*restoreFullscreen)(GLFWwindow*) = nullptr;
};

int runPreview(GLFWwindow* window, PreviewState& preview, const std::string& captureFolder = {},
               PreviewNavigation* navigation = nullptr);
} // namespace school
