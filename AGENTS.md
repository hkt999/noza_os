# Repository Guidelines

## Project Structure & Module Organization
`kernel/` implements the scheduler, IPC, and RP2040 context switch, while `service/` hosts user-space daemons such as memory, sync, VFS, and name lookup. Shared libc, console tools, demo apps, and Unity exercises live under `user/`, with exported headers in `include/` and device helpers in `drivers/`. Keep third-party code isolated in `3rd_party/` and document architectural changes in `doc/` before landing structural refactors.

## Build, Test, and Development Commands
Configure once per workspace: `cmake -S . -B build -DPICO_SDK_PATH=$PICO_SDK_PATH -DNOZAOS_UNITTEST=ON -DNOZAOS_POSIX=ON`. Rebuild via `cmake --build build -j$(sysctl -n hw.ncpu)`; the UF2 image appears at `build/noza.uf2`. Flash with `cd build && picotool load noza.uf2`, and re-run the configure step with extra `-DNOZAOS_* =ON` flags whenever you enable Lua or WS2812 support.

## Coding Style & Naming Conventions
Code targets C11/C++17, four-space indentation, and braces on the declaration line (`void task(void) {`). Prefer `snake_case` identifiers with subsystem prefixes (`noza_thread_self`, `mem_serv_init`) and SCREAMING_SNAKE macros for configuration knobs. Order includes as system → Pico SDK → project headers, keep comments focused on non-obvious scheduler or IPC details, and run `clang-format -style=LLVM` on touched C/C++ files while leaving vendored sources untouched.

## Testing Guidelines
Unity-based suites compile when `NOZAOS_UNITTEST` or `NOZAOS_UNITTEST_POSIX` is enabled; once the image boots, enter `noza_unittest` or `posix_unittest` in the console to run them. Host-side validation is fastest with the POSIX option: execute the produced ELF directly so pthread-backed shims verify without hardware. Add cases under `user/noza_unit_test/` using the `test_<module>_<case>` naming convention and describe any manual hardware steps (GPIO, DMA, timers) in your PR checklist.

## Commit & Pull Request Guidelines
Recent history favors short, imperative subjects (`memory service bug fix`, `refactory service process`), so mirror that voice and keep summaries under ~60 characters. Use commit bodies or PR descriptions to list the CMake flags used, tests executed, and any Pico deployment notes. Every PR should link related issues, highlight breaking changes, attach console logs or screenshots when UX shifts, and rebase on the latest `main` before requesting review.

## Security & Configuration Tips
Never commit UF2 payloads, credentials, or your local `PICO_SDK_PATH`; export the SDK path in your shell profile instead. Treat new services as untrusted: assign unique ports, validate each `noza_call` payload, and cross-check `SECURITY.md` whenever you touch IPC boundaries or memory-sharing rules.
