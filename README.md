# Noza OS
Noza is a lightweight, minimalist microkernel inspired by the mythical figure Noza (哪吒) from Eastern culture, known for being small, agile, and incredibly fast. In the legends, Noza is said to ride the Wind Fire Wheels (風火輪), which allow for swift and effortless movement. The Noza microkernel embodies these qualities, providing a unique and efficient solution for embedded systems and IoT devices. By focusing on core functionality like thread management and IPC, Noza offers a fast and efficient operating system tailored for embedded systems and IoT devices.

# Motivation
The design of Noza is partly inspired by the limitations of some real-time operating systems like FreeRTOS, which do not enforce strict separation between user mode and kernel mode. Noza aims to provide a more secure and robust environment by implementing features such as memory protection and a clear distinction between user and kernel modes, making it a more suitable solution for embedded systems and IoT devices with security and stability concerns.

# Key Features
* Thread management: Noza supports preemptive multithread for SMP core, APIs including thread creation, termination, and synchronization through system calls, enabling efficient multi-threading.
* Inter-process communication (IPC): Noza facilitates communication between threads using message passing, ensuring seamless and synchronized execution of tasks.

## IRQ subsystem (RP2040)
- Kernel delivers HW IRQs to a user-space IRQ service (reserved VID = `IRQ_SERVER_VID`, default 65000). The service re-lays events via `noza_call` so clients can subscribe/unsubscribe per IRQ ID.
- `NOZA_OS_ENABLE_IRQ` is ON; `platform_irq_init` wires UART0 by default. Add more IDs as needed.
- Console now subscribes to UART0 IRQ and becomes IRQ-driven for RX;若服務不可用會自動回退舊的 50ms 輪詢。
- Client helpers: `irq_service_subscribe/unsubscribe` (see `service/irq/irq_client.c`). Events use `noza_irq_event_t`.

# Multicore Scheduling Notes
Noza currently targets dual-core RP2040/RP2350-class MCUs, where ARM Cortex-M cores lack a hardware Inter-Processor Interrupt (IPI) controller. We therefore reuse the RP2040 multicore FIFO as a lightweight software IPI: core A pushes a single 32-bit token, core B's SIO interrupt handler drains the FIFO and sets `PendSV` to enter the scheduler immediately. This approach is officially supported by the Pico SDK, keeps latency low (tens of nanoseconds when the FIFO is not full), and avoids dedicating scarce hardware spinlocks. We evaluated using spinlock-triggered IRQs or SEV/WFE mailboxes, but FIFO offered the best mix of simplicity, deterministic wakeups, and compatibility with existing SDK tooling. Spinlock-based IPIs remain an option if future workloads demand a FIFO dedicated to user-space IPC, yet FIFO signaling is the default because it requires no extra housekeeping and integrates directly with the current scheduling infrastructure.

# Kernel Primitives
To support higher-level APIs (POSIX, pthread, etc.) without bloating the microkernel, Noza now exposes a focused set of synchronization and timing primitives:
* Wait queues & futexes: generic block/unblock infrastructure with microsecond timeouts, plus `noza_futex_wait/wake` syscalls for pthread-style locks and condition variables.
* Timers: lightweight timer objects that can be armed, canceled, and awaited; they share the global systick pipeline so both one-shot and periodic timers wake wait queues precisely.
* Clock queries: `noza_clock_gettime` provides monotonic and realtime timestamps derived from the platform counter.
* Signals: per-thread pending-bit tracking with `noza_signal_send/take` lets runtimes deliver asynchronous events while the kernel handles wakeups of sleeping or blocked threads.

# Process Libc & Naming
RP2040 的 Pico SDK 會拉入 newlib 版本的 `malloc/free/printf/open` 等符號，硬體驅動也依賴這套 libc，因此 Noza 目前採雙層設計：
- **平台層**仍使用 Pico SDK/newlib，確保 USB/UART、硬體驅動與啟動碼可以順利連結與執行。
- **Process 層**提供自有的 `noza_*` API（例如 `noza_process_exec`、`noza_call`、未來的 `noza_open/noza_read/...`）來透過 IPC 與服務互動，並使用 per-process heap allocator（`noza_process_malloc`/`noza_process_free`）。這些名稱刻意避開標準 `malloc/open` 以免和 newlib 符號互相踩踏。
- **Name server** 永遠綁定在 VID 0，所有服務上線時需透過 `name_lookup_register()` 將「名稱 → service_id → VID」對映註冊，客戶端則以 `name_lookup_resolve()` 或 `name_lookup_resolve_id()` 取得最新 VID，無須維護全域 PID。
- 預設 per-process heap 採用 `tinyalloc`，也可以在 CMake 開啟 `-DNOZA_PROCESS_USE_TLSF=ON` 切換到 TLSF（Two-Level Segregated Fit） allocator，以獲得較穩定的配置延遲。兩種 allocator 都共享相同 API，僅影響記憶體管理策略。
- RP2040 只有 32 顆硬體 spinlock，Noza 會在 process 真正被建立時才動態 claim 一顆，再於 process 結束後釋放；保持 `NOZA_MAX_PROCESSES` 在合理範圍（預設 16）即可避免早期耗盡 spinlock 造成開機卡住。
- Application 若需要 POSIX 風格名稱，可以在自己的 header 中選擇 `#define open noza_open` 等別名，但預設請直接使用 `noza_*` 版本，確保呼叫會走到 Noza 的服務層而不是 Pico SDK 的預設 stub。
- 若完全停用 newlib，需自行提供啟動碼、`__aeabi_*` runtime 及 syscall stub，並重新調整 Pico SDK 的 link 過程。本專案暫時維持「平台層 newlib + process 層 Noza libc」的分層方式，以便同時享有硬體支援與 process 隔離。

## Service Naming Workflow
1. **Boot:** `name_server_init()` 以 `noza_add_service_with_vid(..., NAME_SERVER_VID)` 註冊，kernel 會讓第一個服務 thread 直接占用 VID 0，並以 1KB 專用堆疊啟動守護程式，避免其他 daemon 搶走保留號碼。  
2. **Register:** 任何使用 IPC 的服務（memory、sync、VFS …）在進入主迴圈前呼叫 `name_lookup_register(service_name, &service_id)`。第一次會獲得持久 ID，之後重啟可帶著既有 ID 更新 VID，避免客戶端需要硬編新版 PID。  
3. **Resolve:** 使用者態／其他服務改用 `name_lookup_resolve()` 或 `name_lookup_resolve_id()` 取得即時 VID，配合 `noza_call()` 投遞訊息。client 偵測到 RPC 失敗時只要重新 resolve，不必維護全域變數。  
4. **Unregister (optional):** 守護程式在停用或崩潰前可以 `name_lookup_unregister(service_id)`，Name Server 會留住 `service_id` 但標記 VID 為 0，確保下一次註冊能沿用同一代號。

此命名流程確保「名稱、穩定 service_id、當前 VID」三者分離，IPC 連線在 daemon 重啟後能自動復原，也讓 console/unittest 不再需要硬編 PID 或等待特定啟動順序。

# System Calls
The user-space `noza_*` libc wraps all kernel entry points, so higher-level runtimes (POSIX pthread/Lua/console) can stick to one API surface. The current syscall families are:

**Thread & Scheduling**
- `noza_thread_sleep_us/ms()` yield the CPU while arming a wake timer (supports timeouts and remaining time reporting).
- `noza_thread_create()` / `_with_stack()` start a new kernel thread; `_with_stack` accepts caller-supplied stack storage for runtimes that pre-allocate stacks.
- `noza_thread_join()`, `noza_thread_detach()`, `noza_thread_kill()`, `noza_thread_change_priority()`, `noza_thread_self()` and `noza_thread_terminate()` cover lifecycle management.
- `noza_futex_wait()` / `noza_futex_wake()` expose the generic wait-queue primitive so pthread mutex/condvar/spinlock implementations can block without busy loops.

**IPC**
- `noza_call()`, `noza_reply()`, `noza_recv()` implement the synchronous RPC path used by services; non-blocking versions (`noza_nonblock_call/recv`) are available for polling-style servers.

**Timers, clocks, signals**
- `noza_timer_create/arm/wait/cancel/delete()` manage per-process timer handles that use the shared kernel timer wheel.  Both one-shot and periodic timers are supported via `NOZA_TIMER_FLAG_PERIODIC`.
- `noza_clock_gettime()` currently exposes `NOZA_CLOCK_MONOTONIC` and `NOZA_CLOCK_REALTIME`, returning a 64-bit fixed-point timestamp.
- `noza_signal_send()`/`noza_signal_take()` provide a lightweight per-thread signal bitmap used by pthread cancellation and by the unit tests to simulate async events.
- `noza_get_stack_space()` reports the remaining stack bytes of the current thread, useful for debug shells.

**Process management**
- `noza_process_exec()` / `_with_stack()` spawn a new process (user-mode thread tree) and optionally join for its exit code.
- `noza_process_exec_detached()` / `_with_stack()` start background console commands or services without blocking the caller.

These functions all funnel through the internal `noza_syscall(r0,r1,r2,r3)` trampoline, which tags each call with an `NSC_*` identifier before entering the kernel. Higher-level APIs never touch registers directly—use the libc wrappers so argument packing stays in sync with the microkernel.

# Build
- 設好 Pico SDK 路徑（`export PICO_SDK_PATH=...`）。
- 一次性 configure：`cmake -S . -B build -DPICO_SDK_PATH=$PICO_SDK_PATH -DNOZAOS_UNITTEST=ON -DNOZAOS_POSIX=ON`
- 重建：`cmake --build build -j$(sysctl -n hw.ncpu)`
- 韌體：`build/noza.uf2`，以 `picotool load` 燒錄。預設 stdio 走 UART0 (GPIO0/1, 115200)。

# Future works
1. POSIX Style API
2. Virtual file system interface
