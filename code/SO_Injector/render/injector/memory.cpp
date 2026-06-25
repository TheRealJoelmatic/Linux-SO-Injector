//
// memory.cpp — ptrace-based process control wrapper
//

#include "memory.h"
#include "log.h"

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <algorithm>

namespace proc {

    Process::Process(pid_t pid) : pid_(pid) {}

    Process::~Process() {
        if (attached_) detach();
    }

    bool Process::attach() {
        if (ptrace(PTRACE_ATTACH, pid_, nullptr, nullptr) == -1) {
            logger::error("PTRACE_ATTACH(" + std::to_string(pid_) + ") failed: " + strerror(errno));
            return false;
        }

        int status = 0;
        if (waitpid(pid_, &status, WUNTRACED) == -1) {
            logger::error("waitpid failed after attach: " + std::string(strerror(errno)));
            return false;
        }

        if (!WIFSTOPPED(status)) {
            logger::error("process did not stop after attach");
            return false;
        }

        attached_ = true;
        logger::info("attached to pid " + std::to_string(pid_));
        return true;
    }

    bool Process::detach() {
        if (ptrace(PTRACE_DETACH, pid_, nullptr, nullptr) == -1) {
            logger::error("PTRACE_DETACH failed: " + std::string(strerror(errno)));
            return false;
        }
        attached_ = false;
        return true;
    }

    bool Process::resume(int signal) {
        if (ptrace(PTRACE_CONT, pid_, nullptr, reinterpret_cast<void*>(static_cast<intptr_t>(signal))) == -1) {
            logger::error("PTRACE_CONT failed: " + std::string(strerror(errno)));
            return false;
        }
        return true;
    }

    bool Process::getRegs(user_regs_struct& regs) const {
        if (ptrace(PTRACE_GETREGS, pid_, nullptr, &regs) == -1) {
            logger::error("PTRACE_GETREGS failed: " + std::string(strerror(errno)));
            return false;
        }
        return true;
    }

    bool Process::setRegs(const user_regs_struct& regs) {
        if (ptrace(PTRACE_SETREGS, pid_, nullptr,
                   const_cast<user_regs_struct*>(&regs)) == -1) {
            logger::error("PTRACE_SETREGS failed: " + std::string(strerror(errno)));
            return false;
        }
        return true;
    }

    bool Process::readWords(uintptr_t addr, void* buf, size_t size) const {
        const size_t W = sizeof(long);
        auto* dst = static_cast<uint8_t*>(buf);

        for (size_t offset = 0; offset < size; offset += W) {
            errno = 0;
            long word = ptrace(PTRACE_PEEKTEXT, pid_,
                               reinterpret_cast<void*>(addr + offset), nullptr);
            if (errno) {
                logger::error("PTRACE_PEEKTEXT at " + logger::hex(addr + offset) +
                           " failed: " + strerror(errno));
                return false;
            }
            size_t chunk = std::min(W, size - offset);
            memcpy(dst + offset, &word, chunk);
        }
        return true;
    }

    bool Process::writeWords(uintptr_t addr, const void* buf, size_t size) {
        const size_t W = sizeof(long);
        const auto* src = static_cast<const uint8_t*>(buf);

        for (size_t offset = 0; offset < size; offset += W) {
            long word = 0;
            size_t chunk = std::min(W, size - offset);

            if (chunk < W) {
                errno = 0;
                word = ptrace(PTRACE_PEEKTEXT, pid_,
                              reinterpret_cast<void*>(addr + offset), nullptr);
                if (errno) {
                    logger::error("PTRACE_PEEKTEXT (partial) at " + logger::hex(addr + offset) +
                               " failed: " + strerror(errno));
                    return false;
                }
            }

            memcpy(&word, src + offset, chunk);

            if (ptrace(PTRACE_POKETEXT, pid_,
                       reinterpret_cast<void*>(addr + offset),
                       reinterpret_cast<void*>(word)) == -1) {
                logger::error("PTRACE_POKETEXT at " + logger::hex(addr + offset) +
                           " failed: " + strerror(errno));
                return false;
            }
        }
        return true;
    }

} // namespace proc