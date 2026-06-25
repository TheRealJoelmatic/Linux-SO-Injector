#include <iostream>

#include "menu/menu.h"
#include "render/gui.h"

int main() {
    std::cout << "Loading GUI" << std::endl;

    render::startRenderLoop(menu::render);

    std::cout << "Shutting down...." << std::endl;

    return 0;
}
