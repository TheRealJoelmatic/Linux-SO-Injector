// theme_manager.h — save/load theme files under XDG config
#pragma once
#include <string>
#include <vector>

namespace theme_manager {
    std::vector<std::string> listThemes();
    bool saveTheme(const std::string& name);
    bool loadTheme(const std::string& name);
    bool deleteTheme(const std::string& name);
    std::string themesDir();
}
