//
// injector.cpp — Injector tab UI
//

#include <algorithm>
#include <future>
#include <string>
#include <cstring>

#include "tabs.h"
#include "imgui.h"
#include "../../injector/inject.h"
#include "../../injector/config.h"
#include "menu/helpers/messageBox.h"

namespace tabs {

    static std::vector<inject::ProcessInfo> processes;
    static int    selectedPid = -1;
    static char   searchBuffer[128]  = {};
    static char   soPathBuffer[512]  = {};
    static bool   pathLoaded         = false;

    static std::future<std::pair<std::string, bool>> injectFuture;
    static bool injecting = false;

    static void refreshProcesses() {
        processes  = inject::getProcesses();
        selectedPid = -1;
    }

    static bool matchesSearch(const std::string& name, const char* search) {
        if (!search || search[0] == '\0') return true;

        std::string lName = name,  lSearch = search;
        std::transform(lName.begin(),   lName.end(),   lName.begin(),   ::tolower);
        std::transform(lSearch.begin(), lSearch.end(), lSearch.begin(), ::tolower);

        return lName.find(lSearch) != std::string::npos;
    }

    void renderInjector() {
        if (!pathLoaded) {
            pathLoaded = true;
            std::string saved = config::get("last_so_path");
            if (!saved.empty())
                std::strncpy(soPathBuffer, saved.c_str(), sizeof(soPathBuffer) - 1);
        }

        if (ImGui::Button("Refresh ")) refreshProcesses();

        ImGui::Text("");
        ImGui::Separator();
        ImGui::Text("");

        ImGui::Text("Search");
        ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer));
        ImGui::Text("");

        ImGui::BeginChild("proc_list", ImVec2(0, 150), true);
        for (auto& p : processes) {
            if (!matchesSearch(p.name, searchBuffer)) continue;

            std::string label = p.name + "  [" + std::to_string(p.pid) + "]";
            if (ImGui::Selectable(label.c_str(), selectedPid == p.pid))
                selectedPid = p.pid;
        }
        ImGui::EndChild();

        ImGui::Text("");
        ImGui::Separator();
        ImGui::Text("");

        ImGui::Text("Selected PID: %d", selectedPid);
        ImGui::Text("");

        ImGui::Text("SO Path");

        ImGui::InputText("##SO Path", soPathBuffer, sizeof(soPathBuffer));

        const bool canInject = selectedPid > 0 &&
                               soPathBuffer[0] != '\0' &&
                               !injecting;

        if (!canInject) ImGui::BeginDisabled();

        ImGui::Text("");
        if (ImGui::Button("Inject", ImVec2(120, 32))) {
            injecting = true;

            config::set("last_so_path", soPathBuffer);

            const int  pid       = selectedPid;
            const std::string path      = soPathBuffer;
            const std::string entryFunc = entryFuncBuffer;

            injectFuture = std::async(std::launch::async, [pid, path, entryFunc]() {
                return inject::loadSO(pid, path, entryFunc);
            });
            msgBox::showBlocking("Injecting", "Injecting, please wait...");
        }
        if (!canInject) ImGui::EndDisabled();

        if (injecting && injectFuture.valid()) {
            if (injectFuture.wait_for(std::chrono::milliseconds(0)) ==
                std::future_status::ready) {

                injecting = false;
                auto [msg, ok] = injectFuture.get();
                msgBox::closeBlocking();
                msgBox::show(ok ? "Success" : "Failed", msg);
            } else {
                ImGui::SameLine();
                ImGui::Text("Injecting …");
            }
        }
    }

} // namespace tabs
