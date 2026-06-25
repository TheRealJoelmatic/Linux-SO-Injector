# SO_Injector

Lightweight Linux .so injector with an ImGui frontend. Attach to a running process, call `dlopen()` inside it and optionally start an `entry` thread inside the loaded library.

![Inject](https://github.com/TheRealJoelmatic/Linux-SO-Injector/blob/main/imgs/inject.png?raw=true)
![HomeScreen](https://github.com/TheRealJoelmatic/Linux-SO-Injector/blob/main/imgs/advanced.png?raw=true)

Table of Contents
-----------------
- [Quick start](#quick-start)
- [Release binary](#release-binary)
- [Requirements & deps](#requirements--deps)
- [How it works](#How-it-works)
  - [Overview and safety](#overview-and-safety)
  - [High-level flow](#high-level-flow)
  - [Key implementation details and shellcodes](#key-implementation-details-and-shellcodes)
  - [Path page and trampolines](#path-page-and-trampolines)
  - [Why exec region shellcode and stack margin matter](#why-exec-region-shellcode-and-stack-margin-matter)
- [Usage notes](#usage-notes)
- [Troubleshooting](#troubleshooting)
- [Credits / References](#credits--references)

## Quick start
Build and run the GUI:

```bash
git clone https://github.com/TheRealJoelmatic/Linux-SO-Injector.git
cd Linux-SO-Injector/code/SO_Injector
mkdir cmake-build-debug
cd cmake-build-debug
cmake ..
ninja
sudo setcap cap_sys_ptrace,cap_dac_read_search=ep ./SO_Injector
./SO_Injector
```

## Release binary
If you download the prebuilt release binary from GitHub. The Injector must have the cap_sys_ptrace, cap_dac_read_search=eip perms

This can be done so like this
```bash
sudo setcap cap_sys_ptrace,cap_dac_read_search=ep ./SO_Injector
```

## Requirements & deps
- GLFW development headers (libglfw3-dev or equivalent)
- OpenGL development (libGL, libglvnd or mesa dev packages)

- Ensure the binary has a exported function
```cpp
extern "C" void entry() {
    notify("entry() called, injection successful!");
}
```


## How it works

Overview and safety
-------------------
This injector performs in-process `dlopen()` and `dlsym()` calls inside the target process using `ptrace()` to write and execute tiny pieces of machine code (shellcode) inside the target. The approach avoids running complex library-loading sequences from an anonymous mmap page (which can crash glibc), and tries to minimize interference with the target by advancing to a safe syscall-exit stop before injecting.

High-level flow
---------------
1. Resolve absolute path of the supplied `.so` and check whether it is already loaded (scan `/proc/<pid>/maps`).
2. Resolve `dlopen`, `dlsym`, and `pthread_create` addresses in the target via an offset trick: read the local libc base and local symbol address, compute the offset, and add it to the target libc base.
3. Attach to the target with `ptrace` and call `advanceToSyscall()` which performs two `PTRACE_SYSCALL` stops to land at a syscall-exit boundary (safe point where glibc doesn't hold internal locks).
4. Find an executable region in the target (`r-xp` mapping) and back up a small number of bytes (48) to restore later.
5. Write a 3-byte syscall trampoline (`syscall; int3`) into the exec region and use it to invoke `mmap` in the target to obtain a writable path page.
6. Write the `.so` path into the target path page and write `k_dlopen_shellcode` into the exec region; set registers so the shellcode calls `dlopen(path, RTLD_LAZY)` using the target's own stack (with a large extra margin) and execute it.
7. If `dlopen` returned a handle and an entry function was requested, call `dlsym(handle, name)` via `k_dlsym_shellcode` to obtain the entry symbol pointer.
8. If an entry function exists and `pthread_create` is available, create a small RWX trampoline page in the target, write a short stub that calls the entry function and returns `NULL` (so it satisfies the `void*(*)(void*)` thread prototype), then call `pthread_create` from `k_pthread_shellcode`, passing the trampoline as the start routine.
9. Munmap the temporary path page (best-effort), restore the original exec bytes, restore registers, and detach.

Key implementation details and shellcodes
---------------------------------------
The injector uses several small machine-code snippets executed from the target's own executable region. Their high-level purpose and register contract are:

- `k_trampoline` (3 bytes): `syscall; int3`
  - Purpose: Used to perform syscalls (`mmap`, `munmap`) from the target. Writing it into the exec page and invoking it is safe because it uses the kernel syscall path.

- `k_dlopen_shellcode` (~29 bytes):
  - Saves original `rsp` into `r15`, subtracts a large margin (0x20000 = 128 KB) from `rsp` then aligns it, sets `rdi` to the path pointer (`r12`), `rsi` to `RTLD_LAZY` and `call r14` (which is set to the target `dlopen` address). After the call it restores `rsp` and triggers `int3` to return control to the injector.
  - Register contract before resume:
    - `r12` = pathPage address (pointer to .so path)
    - `r14` = target `dlopen` address
    - `rip` = exec trampoline address where `k_dlopen_shellcode` was written

- `k_dlsym_shellcode` (~27 bytes): similar pattern but sets `r12` = libHandle, `r13` = namePtr and `r14` = dlsym address; result returned in `rax`.

- `k_pthread_shellcode` (~33 bytes): sets up args for `pthread_create(pthread_t*, attr, start_routine, arg)` — it places `pthread_t*` in `rdi` (from `r12`), `rsi = NULL`, `rdx = start_routine (r14)`, `rcx = NULL`, and `call r13` where `r13` is the target `pthread_create` address. Uses the same `rsp` margin trick and finishes with `int3`.

Path page and trampolines
------------------------
The injector creates a 4KB `pathPage` inside the target (via `mmap`) with this layout:
- offset 0: the full `.so` path string (used by `dlopen`)
- offset 512: entry function name string (used by `dlsym`)
- offset 768: 8 bytes reserved for `pthread_t` storage

Because `dlopen()` and `dlsym()` must be called in a context where glibc can find the caller's link_map, the dlopen/dlsym shellcodes are executed from the target's existing executable mapping (not the anonymous `mmap` page). Calling from exec region avoids rare crashes where glibc's RETURN_ADDRESS(0) lookup fails when invoked from anonymous pages.

Adapting `extern "C" void entry()` to `pthread_create`
-----------------------------------------------------
`pthread_create` requires a start routine with signature `void*(*)(void*)`. Many user libraries provide `extern "C" void entry()` (no arguments, void return). To safely start such a function as a thread, the injector creates a small RWX stub in the target with machine code equivalent to:

```asm
movabs rax, <entryFuncPtr>
sub    rsp, 8
call   rax
xor    eax, eax    ; return NULL
add    rsp, 8
ret
```

This stub is written into a persistent RWX page in the target and that stub pointer is passed as the `start_routine` to `pthread_create`. The stub handles stack alignment and returns a NULL pointer so `pthread_create` sees a well-formed `void*` return.

Why exec region shellcode and stack margin matter
-----------------------------------------------
- Exec region: Modern glibc uses the caller's return address and link_map to determine symbol namespaces during `dlopen`. Calling `dlopen` from an anonymous mmap page that is not known to the dynamic loader can hit internal code paths that dereference null and crash. Executing shellcode from the target's real exec mapping avoids this.
- Stack margin: `dlopen` triggers complicated glibc internals and deep call chains. Executing `dlopen` from a small private stack (e.g., an `mmap`-allocated stack) can violate glibc's stack-bounds checks or TLS-based assumptions; instead the shellcode saves the real `rsp`, subtracts a large margin (128 KB) to provide extra headroom, aligns, calls, then restores `rsp`.

Concurrency and safety
----------------------
- The injector calls `advanceToSyscall()` right after attaching so the target is stopped at a syscall-exit boundary. This avoids injecting while glibc holds internal locks that could cause deadlocks or crashes.
- The injector backs up the few bytes it overwrites in the exec region and restores them after finishing.

Usage notes
-----------
- The entry function must be exported from the shared object (check with `nm -D libyour.so` or `readelf -s`).
- Use syslog for observable output from the injected library (example `notify()` helper included in `test_So`).
- The injector saves the last-used `.so` path in `~/.config/so_injector/config`.

Troubleshooting
---------------
- If `dlopen` fails: run `ldd libyour.so` and make sure all dependencies available to the target process.
- If injection is refused: check `/proc/sys/kernel/yama/ptrace_scope` or set capabilities as shown in the release section.
- If the entry is not called: confirm symbol name (case-sensitive) and that it is exported (dynamic symbol table).

Credits / References
--------------------
https://github.com/gaffe23/linux-inject
https://github.com/ParkHanbum/linux_so_injector
https://github.com/ilammy/linux-crt
