SO Injector — Quick Start & Internals

Overview
- Purpose: Inject a .so into a running Linux process using ptrace and in-process shellcode to run dlopen/dlsym/pthread_create.
- UI: ImGui frontend that lists processes, lets you pick a PID, select a .so, and optionally run an `entry` function in a new thread.

Quick Use
1. Build the project with CMake/Ninja (already configured):
   - mkdir -p cmake-build-debug && cd cmake-build-debug
   - cmake ..
   - ninja
2. Launch SO_Injector (from `cmake-build-debug/SO_Injector`).
3. In the "Injector" tab:
   - Click Refresh and select the target PID.
   - Enter or paste the full path to the shared object (.so) you want to inject.
   - (Advanced) Set `Entry Function` name (default: `entry`). Leave blank to skip starting a thread.
   - Click `Inject`. The last used path is saved automatically.
4. Watch for success/failure dialog. If your injected library logs to syslog, view with:
   - `journalctl -f -t Test_SO_Injector`

How Injection Works (high level)
- The injector attaches to the target thread with `ptrace` and advances to a syscall-exit boundary to avoid interrupting glibc internal locks.
- It finds a writable page by calling `mmap` in the target via a tiny `syscall; int3` trampoline placed into the target's executable region.
- The library path is written into that page, then a dlopen shellcode (executed from the target's exec region) is invoked to call `dlopen()` inside the target.
- If an entry function is requested, the injector uses `dlsym()` to resolve it, and creates a small RWX trampoline page that adapts `extern "C" void entry()` into the `void*(*)(void*)` signature expected by `pthread_create`.
- The `pthread_create` call is invoked from shellcode in the target so the new thread starts inside the target process.
- The injector cleans up the temporary path page but intentionally leaves the RWX trampoline page in place so the created thread can execute safely after the injector detaches.

Advanced Notes
- Entry function signature:
  - Best: `extern "C" void entry()` — the injector will create a stub so it can be used as a thread start routine.
  - Alternatively, use `extern "C" void* entry(void*)` for direct compatibility.
- Symbol visibility: The entry symbol must be exported in the shared object (check with `nm -D libyour.so` or `readelf -s`).
- Output: `std::cout` from the target goes to the target process's stdout. For reliable visible output use syslog (example helper `notify()` in test library).

Troubleshooting
- "Library already loaded": the injector detects existing library paths in `/proc/<pid>/maps` and will refuse to inject the same path twice.
- `dlopen` fails: check library dependencies and permissions. Running `ldd libyour.so` can help.
- No entry called: ensure the symbol is exported (use `nm -D`) and the entry name matches what you pass in the UI.
- Target crashes: Ensure `ptrace_scope` allows attaching (`/proc/sys/kernel/yama/ptrace_scope`), and that you choose a thread-safe injection moment (the injector advances to a syscall-exit stop to avoid mid-lock injection).

Safety & Legal
- Use responsibly: injecting code into processes can crash them or violate policies. Only inject into processes you own or have explicit permission to modify.

Developer pointers
- Key files:
  - `render/injector/inject.cpp` — core injection logic (ptrace, shellcodes, trampolines)
  - `render/injector/memory.cpp` — low-level ptrace read/write helpers
  - `render/injector/utils.cpp` — /proc/maps helpers and symbol resolution
  - `render/menu/tabs/injector.cpp` — ImGui UI glue
- Tests: A sample test library is in `../test_So` (build there and ensure `entry` is exported).

If you want, I can also:
- Add an in-app "Home" tab that renders this file inside the GUI.
- Add a short example `libexample` project or a unit test harness.

