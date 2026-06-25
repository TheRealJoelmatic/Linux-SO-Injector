//
// Created by Joel on 6/25/26.
//

//
// Created by Joel on 6/23/26.
//

#include <cstdio>
#include <imgui.h>

#include "menu/theme.h"
#include "menu/theme_manager.h"
#include "../../injector/config.h"

namespace tabs {

    const char* getColorName(ImGuiCol idx) {
        switch (idx) {
            case ImGuiCol_Text: return "Text";
            case ImGuiCol_TextDisabled: return "TextDisabled";
            case ImGuiCol_WindowBg: return "WindowBg";
            case ImGuiCol_ChildBg: return "ChildBg";
            case ImGuiCol_PopupBg: return "PopupBg";
            case ImGuiCol_Border: return "Border";
            case ImGuiCol_FrameBg: return "FrameBg";
            case ImGuiCol_FrameBgHovered: return "FrameBgHovered";
            case ImGuiCol_FrameBgActive: return "FrameBgActive";
            case ImGuiCol_TitleBg: return "TitleBg";
            case ImGuiCol_TitleBgActive: return "TitleBgActive";
            case ImGuiCol_Button: return "Button";
            case ImGuiCol_ButtonHovered: return "ButtonHovered";
            case ImGuiCol_ButtonActive: return "ButtonActive";
            case ImGuiCol_Header: return "Header";
            case ImGuiCol_HeaderHovered: return "HeaderHovered";
            case ImGuiCol_HeaderActive: return "HeaderActive";
            case ImGuiCol_Separator: return "Separator";
            case ImGuiCol_ResizeGrip: return "ResizeGrip";
            case ImGuiCol_Tab: return "Tab";
            case ImGuiCol_TabHovered: return "TabHovered";
            case ImGuiCol_TabActive: return "TabActive";
            default: return nullptr;
        }
    }

    void renderThemeEditor() {
        ImGui::Text("Theme Editor");
        ImGui::Text("");
        ImGui::Separator();

        ImGuiStyle& style = ImGui::GetStyle();

        ImGui::Text("");


        if (ImGui::TreeNode("Colors")) {

            for (int i = 0; i < ImGuiCol_COUNT; i++) {
                const char* name = getColorName((ImGuiCol)i);

                char label[64];
                if (name)
                    snprintf(label, sizeof(label), "%s", name);
                else
                    snprintf(label, sizeof(label), "Color %d", i);

                ImGui::ColorEdit4(label, (float*)&style.Colors[i]);
            }

            ImGui::TreePop();
        }

        ImGui::Text("");

        static char themeName[64] = "";
        ImGui::Text("Theme Name");
        ImGui::InputText("##Theme Name", themeName, sizeof(themeName));

        ImGui::Text("");
        if (ImGui::Button("Save Theme")) {
            if (!theme_manager::saveTheme(themeName))
                ImGui::TextDisabled("Failed to save theme (choose a name).");
            else
                ImGui::TextDisabled("Saved theme.");
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Theme")) {
            theme::applyTheme();
        }

        ImGui::Text("");
        ImGui::Separator();
        ImGui::Text("");


        auto themes = theme_manager::listThemes();
        ImGui::Text("Saved Themes:");
        for (size_t i = 0; i < themes.size(); ++i) {
            const std::string& t = themes[i];
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::Text("%s", t.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("Load##") + t).c_str())) {
                if (!theme_manager::loadTheme(t))
                    ImGui::TextDisabled("Failed to load theme.");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("Del##") + t).c_str())) {
                theme_manager::deleteTheme(t);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("SetDefault##") + t).c_str())) {
                config::set("default_theme", t);
            }
        }
    }

    void renderTheme() {
        renderThemeEditor();
    }

}
