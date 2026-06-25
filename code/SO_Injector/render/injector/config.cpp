//
// config.cpp — tiny key/value persistence
//
// File format (one entry per line):
//   key=value
//

#include "config.h"
#include "log.h"

#include <fstream>
#include <unordered_map>
#include <string>
#include <cstdlib>   // getenv
#include <sys/stat.h>

namespace config {

    namespace {
        const std::string& configPath() {
            static std::string path = []() -> std::string {
                const char* xdg = std::getenv("XDG_CONFIG_HOME");
                std::string dir = xdg && xdg[0]
                                ? std::string(xdg) + "/so_injector"
                                : std::string(std::getenv("HOME")) + "/.config/so_injector";

                mkdir(dir.c_str(), 0700);
                return dir + "/config";
            }();
            return path;
        }

        std::unordered_map<std::string, std::string> load() {
            std::unordered_map<std::string, std::string> map;
            std::ifstream f(configPath());
            std::string line;
            while (std::getline(f, line)) {
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                map[line.substr(0, eq)] = line.substr(eq + 1);
            }
            return map;
        }

        void save(const std::unordered_map<std::string, std::string>& map) {
            std::ofstream f(configPath(), std::ios::trunc);
            if (!f) {
                logger::error("config: cannot write to " + configPath());
                return;
            }
            for (auto& [k, v] : map)
                f << k << '=' << v << '\n';
        }

    }

    std::string get(const std::string& key) {
        auto map = load();
        auto it  = map.find(key);
        return it != map.end() ? it->second : std::string{};
    }

    void set(const std::string& key, const std::string& value) {
        auto map    = load();
        map[key]    = value;
        save(map);
    }

} // namespace config
