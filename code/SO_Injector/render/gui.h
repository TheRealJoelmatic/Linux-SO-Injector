//
// Created by Joel on 6/23/26.
//

#ifndef SO_INJECTOR_GUI_H

#define SO_INJECTOR_GUI_H
#include <GLFW/glfw3.h>

namespace render {
    static bool running = true;

    void exampleGui(GLFWwindow* window);
    void startRenderLoop(void (*drawFunction)(GLFWwindow*) = exampleGui);
}

#endif //SO_INJECTOR_GUI_H
