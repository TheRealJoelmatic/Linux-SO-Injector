//
// Created by Joel on 6/23/26.
//

#include "menu.h"
#include "imgui.h"
#include "helpers/messageBox.h"
#include "tabs/tabs.h"

namespace menu {
    bool init = false;

    void render(GLFWwindow* window) {

        if (!init) {
            tabs::initPages();

            init = true;
        }
        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoBackground
        );

        tabs::renderPage();

        msgBox::update();

        ImGui::End();
    }
}
