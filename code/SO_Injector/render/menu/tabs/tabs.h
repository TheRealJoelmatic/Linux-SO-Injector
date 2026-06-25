//
// Created by Joel on 6/23/26.
//

#ifndef SO_INJECTOR_MISC_H
#define SO_INJECTOR_MISC_H
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include "imgui.h"


namespace tabs {

    struct Page {
        std::string name;
        int16_t index;
        void (*func)();
    };

    static char  entryFuncBuffer[128] = "entry";

    static int16_t currentPage = 0;

    inline std::vector<Page> pages;

    void renderTheme();
    void renderHomepage();
    void renderInjector();
    void renderMisc();

    inline void initPages() {
        int16_t index = 0;

        pages.push_back({ "Homepage", index++,renderHomepage });
        pages.push_back({ "Injector", index++, renderInjector });
        pages.push_back({ "Advanced", index++, renderMisc });
        pages.push_back({ "Themes", index++, renderTheme });

        for (auto& page : pages) {
            std::cout << page.name << ": index: " << page.index << std::endl;
        }
    }

    inline void renderPage() {
        ImGui::Text("");
        ImGui::Text("");


        for (auto& page : pages) {
            std::string name = page.name;

            ImGui::SameLine();
            if (ImGui::Button(name.c_str(), ImVec2(120, 40))) {
                currentPage = page.index;
            }
        }

        ImGui::Text("");
        ImGui::Separator();
        ImGui::Text("");

        for (auto& page : pages) {
            if (page.index == currentPage) {
                page.func();
            }
        }
    }
}



#endif //SO_INJECTOR_MISC_H
