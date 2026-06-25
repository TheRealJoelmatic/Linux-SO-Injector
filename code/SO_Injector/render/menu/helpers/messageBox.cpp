//
// Created by Joel on 6/24/26.
//

#include "messageBox.h"
#include "imgui.h"
#include <chrono>

namespace msgBox
{
    struct MessageBoxState
    {
        bool active = false;
        std::string title;
        std::string text;

        bool hasTimer = false;
        int autoCloseMs = 0;

        std::chrono::steady_clock::time_point startTime;
        bool blocking = false;
    };

    static MessageBoxState state;

    void show(const std::string& title,
              const std::string& text,
              int autoCloseMs)
    {
        state.active = true;
        state.title = title;
        state.text = text;

        state.autoCloseMs = autoCloseMs;
        state.hasTimer = (autoCloseMs > 0);

        state.startTime = std::chrono::steady_clock::now();

        ImGui::OpenPopup(state.title.c_str());
    }

    void showBlocking(const std::string& title, const std::string& text)
    {
        state.active = true;
        state.blocking = true;
        state.title = title;
        state.text = text;
        state.hasTimer = false;
        ImGui::OpenPopup(state.title.c_str());
    }

    void closeBlocking()
    {
        if (state.blocking) {
            state.blocking = false;
            state.active = false;
            ImGui::CloseCurrentPopup();
        }
    }

    bool isOpen()
    {
        return state.active;
    }

    void update()
    {
        if (!state.active)
            return;

        if (state.hasTimer)
        {
            auto now = std::chrono::steady_clock::now();
            int elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - state.startTime
                ).count();

            if (elapsed >= state.autoCloseMs)
            {
                state.active = false;
                ImGui::CloseCurrentPopup();
                return;
            }
        }

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f,
                               io.DisplaySize.y * 0.5f);

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Appearing);

        // If blocking and the user managed to close the popup (e.g. with Esc),
        // reopen it immediately to keep it non-closable until closeBlocking().
        if (state.blocking && !ImGui::IsPopupOpen(state.title.c_str()))
            ImGui::OpenPopup(state.title.c_str());

        if (ImGui::BeginPopupModal(state.title.c_str(),
                                   nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::TextWrapped("%s", state.text.c_str());

            ImGui::Spacing();

            if (!state.blocking)
            {
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    state.active = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
        else
        {
            if (!ImGui::IsPopupOpen(state.title.c_str()))
                state.active = false;
        }
    }
}