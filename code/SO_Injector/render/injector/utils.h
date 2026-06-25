#pragma once

#include <string>
#include <cstdint>
#include <sys/types.h>

namespace utils {

    uintptr_t getExecRegion(pid_t pid);
    uintptr_t getLibcBase(pid_t pid);
    bool isLibraryLoaded(pid_t pid, const std::string& libName);
    uintptr_t getLocalSymbol(const std::string& name);

} // namespace utils