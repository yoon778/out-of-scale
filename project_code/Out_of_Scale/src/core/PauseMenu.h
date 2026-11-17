#pragma once

struct GLFWwindow;

class PauseMenu {
public:
    PauseMenu() = default;
    PauseMenu(const PauseMenu&) = delete;
    PauseMenu& operator=(const PauseMenu&) = delete;
    ~PauseMenu();

    bool initialize();
    bool isPaused() const;
    void setPaused(bool paused);
    bool keyPressed(GLFWwindow* window, int key);
    void click(GLFWwindow* window);
    void draw(GLFWwindow* window, int width, int height) const;

private:
    bool paused_ = false;
    unsigned int program_ = 0;
    unsigned int vertexArray_ = 0;
    unsigned int texture_ = 0;
    int panelLocation_ = -1;
    int heightLocation_ = -1;
    int hoverLocation_ = -1;
};
