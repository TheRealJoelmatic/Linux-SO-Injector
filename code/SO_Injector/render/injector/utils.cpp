//
// utils.cpp — /proc/maps helpers and symbol resolution
//

#include "utils.h"
#include "log.h"

#include <fstream>
#include <string>
#include <dlfcn.h>

namespace utils {

    uintptr_t getExecRegion(pid_t pid) {
        std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
        std::string line;

        while (std::getline(maps, line)) {
            if (line.find("r-xp") == std::string::npos) continue;

            auto dash = line.find('-');
            if (dash == std::string::npos) continue;

            uintptr_t addr = std::stoull(line.substr(0, dash), nullptr, 16);
            logger::debug("exec region in pid " + std::to_string(pid) + ": " + logger::hex(addr));
            return addr;
        }

        logger::error("no executable region found in pid " + std::to_string(pid));
        return 0;
    }

    uintptr_t getLibcBase(pid_t pid) {
        std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
        std::string line;

        while (std::getline(maps, line)) {
            bool hasLibc = line.find("libc.so") != std::string::npos ||
                           line.find("libc-")   != std::string::npos;
            if (!hasLibc) continue;

            auto dash = line.find('-');
            if (dash == std::string::npos) continue;

            uintptr_t addr = std::stoull(line.substr(0, dash), nullptr, 16);
            logger::debug("libc base in pid " + std::to_string(pid) + ": " + logger::hex(addr));
            return addr;
        }

        logger::error("libc not found in pid " + std::to_string(pid) + " maps");
        return 0;
    }

    bool isLibraryLoaded(pid_t pid, const std::string& libName) {
        std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
        std::string line;

        while (std::getline(maps, line)) {
            if (line.find(libName) != std::string::npos)
                return true;
        }
        return false;
    }

    uintptr_t getLocalSymbol(const std::string& name) {
        void* handle = dlopen("libc.so.6", RTLD_LAZY | RTLD_NOLOAD);
        if (!handle)
            handle = dlopen("libc.so.6", RTLD_LAZY);
        if (!handle) {
            logger::error("dlopen(libc.so.6) failed: " + std::string(dlerror()));
            return 0;
        }

        dlerror();
        void* sym = dlsym(handle, name.c_str());
        const char* err = dlerror();
        if (err) {
            logger::error("dlsym(" + name + ") failed: " + err);
            return 0;
        }

        return reinterpret_cast<uintptr_t>(sym);
    }

} // namespace utils