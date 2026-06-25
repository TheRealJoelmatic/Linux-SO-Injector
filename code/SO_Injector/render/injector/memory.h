//
// memory.h — ptrace-based process control wrapper
//

#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/types.h>
#include <sys/user.h>

namespace proc {

class Process {
public:
    explicit Process(pid_t pid);
    ~Process();

    bool attach();
    bool detach();

    bool resume(int signal = 0);

    bool getRegs(user_regs_struct& regs) const;
    bool setRegs(const user_regs_struct& regs);

    bool readWords(uintptr_t addr, void* buf, size_t size) const;
    bool writeWords(uintptr_t addr, const void* buf, size_t size);

    pid_t getPid()      const { return pid_;      }
    bool  isAttached()  const { return attached_;  }

private:
    pid_t pid_;
    bool  attached_ = false;
};

} // namespace proc