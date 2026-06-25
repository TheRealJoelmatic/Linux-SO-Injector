#include "theme_manager.h"
#include "theme.h"
#include "../injector/config.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace theme_manager {

std::string themesDir() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::string dir = xdg && xdg[0]
                    ? std::string(xdg) + "/so_injector/themes"
                    : std::string(std::getenv("HOME")) + "/.config/so_injector/themes";
    fs::create_directories(dir);
    return dir;
}

std::vector<std::string> listThemes() {
    std::vector<std::string> out;
    try {
        for (auto& p : fs::directory_iterator(themesDir())) {
            if (!p.is_regular_file()) continue;
            auto name = p.path().filename().string();
            if (p.path().extension() == ".theme")
                out.push_back(p.path().stem().string());
        }
    } catch (...) {}
    return out;
}

bool saveTheme(const std::string& name) {
    if (name.empty()) return false;
    if (name == "Stock") return false;
    std::string path = themesDir() + "/" + name + ".theme";
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    ImGuiStyle& style = ImGui::GetStyle();
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        auto& c = style.Colors[i];
        f << std::setprecision(8) << c.x << ' ' << c.y << ' ' << c.z << ' ' << c.w << '\n';
    }
    return true;
}

bool loadTheme(const std::string& name) {
    if (name.empty()) return false;
    if (name == "Stock") { theme::applyTheme(); return true; }
    std::string path = themesDir() + "/" + name + ".theme";
    std::ifstream f(path);
    if (!f) return false;
    ImGuiStyle& style = ImGui::GetStyle();
    std::string line;
    int idx = 0;
    while (std::getline(f, line) && idx < ImGuiCol_COUNT) {
        std::istringstream ss(line);
        float a, b, c_, d;
        if (!(ss >> a >> b >> c_ >> d)) return false;
        style.Colors[idx++] = ImVec4(a, b, c_, d);
    }
    return idx == ImGuiCol_COUNT;
}

bool deleteTheme(const std::string& name) {
    if (name.empty()) return false;
    if (name == "Stock") return false; // cannot delete built-in theme
    std::string path = themesDir() + "/" + name + ".theme";
    try { return fs::remove(path); } catch (...) { return false; }
}

} // namespace theme_manager
