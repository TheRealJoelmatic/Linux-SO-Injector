//
// Created by Joel on 6/23/26.
//

#include "tabs.h"

namespace tabs {
    void renderHomepage(){

        ImGui::BeginChild("home_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_None);

        ImGui::TextWrapped("Quick Start & Internals");
        ImGui::BulletText("Purpose: Inject a .so into a running process");
        ImGui::BulletText("Select a process, pick a .so path, optionally set an entry function, then click Inject.");

        ImGui::Text("");
        ImGui::Separator();
        ImGui::Text("");

        ImGui::Text("notes:");
        ImGui::BulletText("Entry symbol must be exported from the .so");
        ImGui::BulletText("BTW you can check this with `nm -D libyour.so`.");


        ImGui::EndChild();
    }
}