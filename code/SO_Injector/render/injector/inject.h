//
// inject.h — public API for remote .so injection
//

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace inject {

struct ProcessInfo {
    int         pid;
    std::string name;
};

std::vector<ProcessInfo> getProcesses();

std::pair<std::string, bool> loadSO(int pid, const std::string& soPath,
                                     const std::string& entryFunc = "entry");

} // namespace inject
