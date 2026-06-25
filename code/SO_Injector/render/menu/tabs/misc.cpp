//
// Created by Joel on 6/23/26.
//

#include <cstdio>
#include <cstring>
#include <imgui.h>
#include "tabs.h"

namespace tabs {

    void renderMisc() {

        ImGui::Text("");
        ImGui::Text("Entry Function");
        ImGui::InputText(" ", entryFuncBuffer, sizeof(entryFuncBuffer));
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) {
            std::strncpy(entryFuncBuffer, "entry", sizeof(entryFuncBuffer));
        }
        ImGui::Text("");
        ImGui::TextDisabled("Called as a new thread after injection.");
        ImGui::Spacing();

    }

}
