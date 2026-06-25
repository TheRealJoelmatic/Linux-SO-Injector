//
// log.h — simple console logger for SO_Injector
//

#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>

namespace logger {

inline std::string hex(uintptr_t v) {
    std::ostringstream ss;
    ss << "0x" << std::hex << v;
    return ss.str();
}

#ifdef NDEBUG
constexpr bool kDebugEnabled = false;
#else
constexpr bool kDebugEnabled = true;
#endif

inline void debug(const std::string& msg) {
    if constexpr (kDebugEnabled)
        std::cout << "[~] " << msg << "\n";
}

inline void info(const std::string& msg) {
    std::cout << "[+] " << msg << "\n";
}

inline void warn(const std::string& msg) {
    std::cout << "[!] " << msg << "\n";
}

inline void error(const std::string& msg) {
    std::cerr << "[-] " << msg << "\n" << std::flush;
}

} // namespace log
