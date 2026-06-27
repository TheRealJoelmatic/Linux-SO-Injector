//
// inject.cpp
//

#include "inject.h"
#include "memory.h"
#include "utils.h"
#include "log.h"

#include <dirent.h>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <climits>
#include <csignal>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>

namespace inject {

    // Syscall trampoline
    static constexpr uint8_t k_trampoline[] = { 0x0f, 0x05, 0xcc };


    //   49 89 E7              mov  r15, rsp          save original rsp
    //   48 81 EC 00 00 02 00  sub  rsp, 0x20000      128 KB margin for dlopen
    //   48 83 E4 F0           and  rsp, -16          16-byte alignment (SysV ABI)
    //   4C 89 E7              mov  rdi, r12          arg0 = path
    //   BE 01 00 00 00        mov  esi, 1            arg1 = RTLD_LAZY
    //   41 FF D6              call r14               dlopen → rax = handle
    //   4C 89 FC              mov  rsp, r15          restore rsp
    //   CC                    int3                   STOP
    static constexpr uint8_t k_dlopen_shellcode[] = {
        0x49, 0x89, 0xe7,                           // mov  r15, rsp
        0x48, 0x81, 0xec, 0x00, 0x00, 0x02, 0x00,  // sub  rsp, 0x20000
        0x48, 0x83, 0xe4, 0xf0,                     // and  rsp, -16
        0x4c, 0x89, 0xe7,                           // mov  rdi, r12
        0xbe, 0x01, 0x00, 0x00, 0x00,               // mov  esi, 1
        0x41, 0xff, 0xd6,                           // call r14
        0x4c, 0x89, 0xfc,                           // mov  rsp, r15
        0xcc,                                        // int3
    };
    static_assert(sizeof(k_dlopen_shellcode) == 29, "shellcode size mismatch");


    //   49 89 E7              mov  r15, rsp
    //   48 81 EC 00 00 02 00  sub  rsp, 0x20000
    //   48 83 E4 F0           and  rsp, -16
    //   4C 89 E7              mov  rdi, r12   (handle)
    //   4C 89 EE              mov  rsi, r13   (name ptr)
    //   41 FF D6              call r14        (dlsym → rax = func ptr)
    //   4C 89 FC              mov  rsp, r15
    //   CC                    int3
    static constexpr uint8_t k_dlsym_shellcode[] = {
        0x49, 0x89, 0xe7,                           // mov  r15, rsp
        0x48, 0x81, 0xec, 0x00, 0x00, 0x02, 0x00,  // sub  rsp, 0x20000
        0x48, 0x83, 0xe4, 0xf0,                     // and  rsp, -16
        0x4c, 0x89, 0xe7,                           // mov  rdi, r12   (handle)
        0x4c, 0x89, 0xee,                           // mov  rsi, r13   (name ptr)
        0x41, 0xff, 0xd6,                           // call r14        (dlsym)
        0x4c, 0x89, 0xfc,                           // mov  rsp, r15
        0xcc,                                        // int3
    };
    static_assert(sizeof(k_dlsym_shellcode) == 27, "dlsym shellcode size mismatch");


    //   49 89 E7              mov  r15, rsp
    //   48 81 EC 00 00 02 00  sub  rsp, 0x20000
    //   48 83 E4 F0           and  rsp, -16
    //   4C 89 E7              mov  rdi, r12
    //   48 31 F6              xor  rsi, rsi
    //   4C 89 F2              mov  rdx, r14
    //   48 31 C9              xor  rcx, rcx
    //   41 FF D5              call r13        (pthread_create → rax = 0 on success)
    //   4C 89 FC              mov  rsp, r15
    //   CC                    int3
    // ---------------------------------------------------------------------------
    static constexpr uint8_t k_pthread_shellcode[] = {
        0x49, 0x89, 0xe7,                           // mov  r15, rsp
        0x48, 0x81, 0xec, 0x00, 0x00, 0x02, 0x00,  // sub  rsp, 0x20000
        0x48, 0x83, 0xe4, 0xf0,                     // and  rsp, -16
        0x4c, 0x89, 0xe7,                           // mov  rdi, r12   (pthread_t* storage)
        0x48, 0x31, 0xf6,                           // xor  rsi, rsi   (NULL attr)
        0x4c, 0x89, 0xf2,                           // mov  rdx, r14   (start_routine)
        0x48, 0x31, 0xc9,                           // xor  rcx, rcx   (NULL arg)
        0x41, 0xff, 0xd5,                           // call r13        (pthread_create)
        0x4c, 0x89, 0xfc,                           // mov  rsp, r15
        0xcc,                                        // int3
    };
    static_assert(sizeof(k_pthread_shellcode) == 33, "pthread shellcode size mismatch");

    static constexpr size_t k_pathPageSize         = 0x1000;
    static constexpr size_t k_pathPageFuncOffset   = 512;
    static constexpr size_t k_pathPageThreadOffset = 768;

    static constexpr size_t k_backupSize = 48;


    static void advanceToSyscall(pid_t pid) {
        ptrace(PTRACE_SETOPTIONS, pid, nullptr,
               reinterpret_cast<void*>(static_cast<long>(PTRACE_O_TRACESYSGOOD)));

        for (int pass = 0; pass < 2; ++pass) {
            if (ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr) == -1) {
                logger::warn("PTRACE_SYSCALL failed (pass " + std::to_string(pass) +
                             "): " + strerror(errno));
                return;
            }
            int status = 0;
            if (waitpid(pid, &status, 0) == -1 || !WIFSTOPPED(status)) {
                logger::warn("unexpected waitpid during syscall advancement");
                return;
            }
            if (WSTOPSIG(status) != (SIGTRAP | 0x80)) {
                logger::warn("non-syscall stop signal " +
                             std::to_string(WSTOPSIG(status)) + " during advancement");
                return;
            }
        }
        logger::debug("advanced past syscall boundary");
    }

    static int resumeAndWait(proc::Process& proc) {
        int signal = 0;

        for (;;) {
            if (!proc.resume(signal))
                return -1;

            int status = 0;
            if (waitpid(proc.getPid(), &status, 0) == -1) {
                logger::error("waitpid failed: " + std::string(strerror(errno)));
                return -1;
            }

            if (WIFSIGNALED(status)) {
                logger::error("target killed by signal " + std::to_string(WTERMSIG(status)));
                return -1;
            }

            if (WIFSTOPPED(status)) {
                int sig = WSTOPSIG(status);
                if (sig == SIGTRAP)
                    return SIGTRAP;

                if (sig == SIGSEGV || sig == SIGBUS ||
                    sig == SIGILL  || sig == SIGABRT) {
                    logger::error("target crashed: signal " + std::to_string(sig) +
                                  " (" + strsignal(sig) + ")");
                    return sig;
                }

                logger::debug("forwarding signal " + std::to_string(sig));
                signal = sig;
                continue;
            }

            return -1;
        }
    }

    std::vector<ProcessInfo> getProcesses() {
        std::vector<ProcessInfo> result;

        DIR* dir = opendir("/proc");
        if (!dir) {
            logger::error("cannot open /proc: " + std::string(strerror(errno)));
            return result;
        }

        dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;

            const std::string pidStr = entry->d_name;
            if (pidStr.empty() ||
                !std::all_of(pidStr.begin(), pidStr.end(), ::isdigit))
                continue;

            std::string name;
            std::ifstream cmdline("/proc/" + pidStr + "/cmdline");
            if (cmdline.is_open())
                std::getline(cmdline, name, '\0');

            if (name.empty())
                name = pidStr;

            if (auto slash = name.rfind('/'); slash != std::string::npos)
                name = name.substr(slash + 1);

            result.push_back({ std::stoi(pidStr), name });
        }

        closedir(dir);
        return result;
    }

    std::pair<std::string, bool> loadSO(int pid, const std::string& rawPath,
                                         const std::string& entryFunc) {
        char resolvedPath[PATH_MAX] = {};
        if (!realpath(rawPath.c_str(), resolvedPath)) {
            return { "Cannot resolve path '" + rawPath + "': " + strerror(errno), false };
        }

        const size_t pathLen = strlen(resolvedPath) + 1;

        logger::info("inject: '" + std::string(resolvedPath) + "' → pid " + std::to_string(pid));

        if (utils::isLibraryLoaded(pid, resolvedPath)) {
            return { "Library is already loaded in that process.", false };
        }

        const uintptr_t localLibcBase = utils::getLibcBase(getpid());
        if (!localLibcBase)
            return { "Failed to locate local libc base.", false };

        uintptr_t dlopenLocal = utils::getLocalSymbol("dlopen");
        if (!dlopenLocal) {
            dlopenLocal = utils::getLocalSymbol("__libc_dlopen_mode");
            if (!dlopenLocal)
                return { "Failed to resolve dlopen.", false };
        }

        const uintptr_t dlsymLocal          = utils::getLocalSymbol("dlsym");
        const uintptr_t pthreadCreateLocal  = utils::getLocalSymbol("pthread_create");

        logger::debug("local libc base      : " + logger::hex(localLibcBase));
        logger::debug("local dlopen         : " + logger::hex(dlopenLocal));
        logger::debug("local dlsym          : " + logger::hex(dlsymLocal));
        logger::debug("local pthread_create : " + logger::hex(pthreadCreateLocal));

        const uintptr_t targetLibcBase = utils::getLibcBase(pid);
        if (!targetLibcBase)
            return { "Failed to locate libc in target process.", false };

        const uintptr_t targetDlopen        = targetLibcBase + (dlopenLocal       - localLibcBase);
        const uintptr_t targetDlsym         = dlsymLocal
                                              ? targetLibcBase + (dlsymLocal         - localLibcBase)
                                              : 0;
        const uintptr_t targetPthreadCreate = pthreadCreateLocal
                                              ? targetLibcBase + (pthreadCreateLocal - localLibcBase)
                                              : 0;

        logger::debug("target libc base     : " + logger::hex(targetLibcBase));
        logger::debug("target dlopen        : " + logger::hex(targetDlopen));
        logger::debug("target dlsym         : " + logger::hex(targetDlsym));
        logger::debug("target pthread_create: " + logger::hex(targetPthreadCreate));

        proc::Process process(pid);
        if (!process.attach())
            return { "Failed to attach (did not have perms process requires cap_sys_ptrace,cap_dac_read_search=eip)", false };

        advanceToSyscall(pid);

        user_regs_struct oldRegs{};
        if (!process.getRegs(oldRegs)) {
            process.detach();
            return { "Failed to read target registers.", false };
        }

        uintptr_t trampolineAddr = utils::getExecRegion(pid);
        if (!trampolineAddr) {
            process.detach();
            return { "No executable region found in target.", false };
        }
        trampolineAddr += sizeof(long);
        logger::debug("trampoline at        : " + logger::hex(trampolineAddr));

        uint8_t backup[k_backupSize];
        if (!process.readWords(trampolineAddr, backup, k_backupSize)) {
            process.detach();
            return { "Failed to back up exec region.", false };
        }

        auto failClean = [&](const std::string& msg) -> std::pair<std::string, bool> {
            process.writeWords(trampolineAddr, backup, k_backupSize);
            process.setRegs(oldRegs);
            process.detach();
            return { msg, false };
        };

        if (!process.writeWords(trampolineAddr, k_trampoline, sizeof(k_trampoline)))
            return failClean("Failed to write mmap trampoline.");

        user_regs_struct mmapRegs = oldRegs;
        mmapRegs.rax = 9;
        mmapRegs.rdi = 0;
        mmapRegs.rsi = k_pathPageSize;
        mmapRegs.rdx = 3;                                        // PROT_READ|PROT_WRITE
        mmapRegs.r10 = 0x22;                                     // MAP_PRIVATE|MAP_ANON
        mmapRegs.r8  = static_cast<unsigned long long>(-1LL);
        mmapRegs.r9  = 0;
        mmapRegs.rip = trampolineAddr;

        if (!process.setRegs(mmapRegs))
            return failClean("Failed to set mmap registers.");

        logger::info("running target → mmap …");
        if (resumeAndWait(process) != SIGTRAP)
            return failClean("Unexpected stop at mmap stage.");

        user_regs_struct afterMmap{};
        if (!process.getRegs(afterMmap))
            return failClean("Failed to read registers after mmap.");

        const uintptr_t pathPage = afterMmap.rax;
        if (static_cast<long long>(pathPage) < 0)
            return failClean("mmap() failed in target (errno=" +
                             std::to_string(-static_cast<long long>(pathPage)) + ").");

        logger::info("path page at         : " + logger::hex(pathPage));

        if (!process.writeWords(pathPage, resolvedPath, pathLen))
            return failClean("Failed to write path to target.");

        if (!process.writeWords(trampolineAddr, k_dlopen_shellcode, sizeof(k_dlopen_shellcode)))
            return failClean("Failed to write dlopen shellcode.");

        user_regs_struct dlopenRegs = oldRegs;
        dlopenRegs.r12 = pathPage;
        dlopenRegs.r14 = targetDlopen;
        dlopenRegs.rip = trampolineAddr;

        if (!process.setRegs(dlopenRegs))
            return failClean("Failed to set dlopen registers.");

        logger::info("running target → dlopen …");
        if (resumeAndWait(process) != SIGTRAP)
            return failClean("dlopen stage crashed — check library dependencies and permissions.");

        user_regs_struct afterDlopen{};
        if (!process.getRegs(afterDlopen))
            return failClean("Failed to read registers after dlopen.");

        const uintptr_t libHandle = afterDlopen.rax;
        if (!libHandle)
            logger::error("dlopen returned NULL — check library and its dependencies");
        else
            logger::info("dlopen handle        : " + logger::hex(libHandle));

        std::string resultMsg;
        bool threadStarted = false;

        if (libHandle && !entryFunc.empty()) {
            if (!targetDlsym)         logger::warn("dlsym not found — cannot look up entry function");
            if (!targetPthreadCreate) logger::warn("pthread_create not found — cannot start thread");
        }

        const bool doThread = libHandle && !entryFunc.empty() && targetDlsym && targetPthreadCreate;

        if (doThread) {
            auto runThread = [&]() {

                const size_t funcNameLen = entryFunc.size() + 1;
                if (!process.writeWords(pathPage + k_pathPageFuncOffset,
                                        entryFunc.c_str(), funcNameLen)) {
                    logger::warn("failed to write entry func name — skipping thread start");
                    return;
                }

                if (!process.writeWords(trampolineAddr, k_dlsym_shellcode,
                                        sizeof(k_dlsym_shellcode))) {
                    logger::warn("failed to write dlsym shellcode");
                    return;
                }

                user_regs_struct dlsymRegs = oldRegs;
                dlsymRegs.r12 = libHandle;
                dlsymRegs.r13 = pathPage + k_pathPageFuncOffset;
                dlsymRegs.r14 = targetDlsym;
                dlsymRegs.rip = trampolineAddr;

                if (!process.setRegs(dlsymRegs)) {
                    logger::warn("failed to set dlsym registers");
                    return;
                }

                logger::info("running target → dlsym(\"" + entryFunc + "\") …");
                if (resumeAndWait(process) != SIGTRAP) {
                    logger::warn("dlsym stage crashed");
                    return;
                }

                user_regs_struct afterDlsym{};
                if (!process.getRegs(afterDlsym)) return;

                const uintptr_t entryFuncPtr = afterDlsym.rax;
                if (!entryFuncPtr) {
                    logger::warn("dlsym returned NULL — '" + entryFunc + "' not found");
                    resultMsg = "Injected but entry function '" + entryFunc + "' was not found.";
                    return;
                }

                logger::info("entry func at        : " + logger::hex(entryFuncPtr));

                if (!process.writeWords(trampolineAddr, k_trampoline, sizeof(k_trampoline))) {
                    logger::warn("failed to write trampoline for trampoline-page mmap");
                    return;
                }

                user_regs_struct tpMmapRegs = oldRegs;
                tpMmapRegs.rax = 9;                                       // SYS_mmap
                tpMmapRegs.rdi = 0;
                tpMmapRegs.rsi = 0x1000;
                tpMmapRegs.rdx = 7;                                       // PROT_RWX
                tpMmapRegs.r10 = 0x22;                                    // MAP_PRIVATE|MAP_ANON
                tpMmapRegs.r8  = static_cast<unsigned long long>(-1LL);
                tpMmapRegs.r9  = 0;
                tpMmapRegs.rip = trampolineAddr;

                if (!process.setRegs(tpMmapRegs)) return;

                logger::info("running target → mmap (thread trampoline page) …");
                if (resumeAndWait(process) != SIGTRAP) {
                    logger::warn("mmap for trampoline page failed");
                    return;
                }

                user_regs_struct afterTpMmap{};
                if (!process.getRegs(afterTpMmap)) return;

                const uintptr_t tPage = afterTpMmap.rax;
                if (static_cast<long long>(tPage) < 0) {
                    logger::warn("trampoline page mmap failed errno=" +
                                 std::to_string(-static_cast<long long>(tPage)));
                    return;
                }

                logger::info("trampoline page      : " + logger::hex(tPage));

                //   48 B8 xx xx xx xx xx xx xx xx  movabs rax, <entryFuncPtr>
                //   48 83 EC 08                    sub    rsp, 8
                //   FF D0                          call   rax
                //   31 C0                          xor    eax, eax   (return NULL)
                //   48 83 C4 08                    add    rsp, 8
                //   C3                             ret

                uint8_t stub[23] = {
                    0x48, 0xb8,                              // movabs rax, imm64
                    0,0,0,0,0,0,0,0,                         // <funcPtr patched below>
                    0x48, 0x83, 0xec, 0x08,                  // sub rsp, 8
                    0xff, 0xd0,                               // call rax
                    0x31, 0xc0,                               // xor eax, eax
                    0x48, 0x83, 0xc4, 0x08,                  // add rsp, 8
                    0xc3,                                     // ret
                };
                memcpy(stub + 2, &entryFuncPtr, 8);

                if (!process.writeWords(tPage, stub, sizeof(stub))) {
                    logger::warn("failed to write thread trampoline to tPage");
                    return;
                }

                if (!process.writeWords(trampolineAddr, k_pthread_shellcode,
                                        sizeof(k_pthread_shellcode))) {
                    logger::warn("failed to write pthread shellcode");
                    return;
                }

                user_regs_struct pthreadRegs = oldRegs;
                pthreadRegs.r12 = pathPage + k_pathPageThreadOffset;
                pthreadRegs.r13 = targetPthreadCreate;
                pthreadRegs.r14 = tPage;        // stub is the start_routine
                pthreadRegs.rip = trampolineAddr;

                if (!process.setRegs(pthreadRegs)) return;

                logger::info("running target → pthread_create …");
                if (resumeAndWait(process) != SIGTRAP) {
                    logger::warn("pthread_create stage crashed");
                    return;
                }

                user_regs_struct afterPthread{};
                if (!process.getRegs(afterPthread)) return;

                if (afterPthread.rax == 0) {
                    logger::info("thread started successfully");
                    threadStarted = true;
                } else {
                    logger::warn("pthread_create returned errno=" +
                                 std::to_string(afterPthread.rax));
                }
            };

            runThread();
        }

        if (process.writeWords(trampolineAddr, k_trampoline, sizeof(k_trampoline))) {
            user_regs_struct munmapRegs = oldRegs;
            munmapRegs.rax = 11;             // SYS_munmap
            munmapRegs.rdi = pathPage;
            munmapRegs.rsi = k_pathPageSize;
            munmapRegs.rip = trampolineAddr;

            if (process.setRegs(munmapRegs)) {
                logger::info("running target → munmap …");
                if (resumeAndWait(process) == SIGTRAP)
                    logger::debug("munmap done");
                else
                    logger::warn("munmap stop unexpected — path page may leak");
            }
        }

        process.writeWords(trampolineAddr, backup, k_backupSize);
        process.setRegs(oldRegs);
        process.detach();

        if (!libHandle)
            return { "dlopen returned NULL — library failed to load.", false };

        if (!resultMsg.empty())
            return { resultMsg, true };

        const bool verified = utils::isLibraryLoaded(pid, resolvedPath);
        const std::string libName = verified ? std::string(resolvedPath) : "library";

        if (threadStarted)
            return { "Injected & thread started: " + std::string(resolvedPath), true };

        if (!entryFunc.empty())
            return { "Injected: " + std::string(resolvedPath) +
                     " — entry function '" + entryFunc + "' not found.", true };

        return { "Injected: " + std::string(resolvedPath), true };
    }

} // namespace inject
