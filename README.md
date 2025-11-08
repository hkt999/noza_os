# Noza OS
Noza is a lightweight, minimalist microkernel inspired by the mythical figure Noza (哪吒) from Eastern culture, known for being small, agile, and incredibly fast. In the legends, Noza is said to ride the Wind Fire Wheels (風火輪), which allow for swift and effortless movement. The Noza microkernel embodies these qualities, providing a unique and efficient solution for embedded systems and IoT devices. By focusing on core functionality like thread management and IPC, Noza offers a fast and efficient operating system tailored for embedded systems and IoT devices.

# Motivation
The design of Noza is partly inspired by the limitations of some real-time operating systems like FreeRTOS, which do not enforce strict separation between user mode and kernel mode. Noza aims to provide a more secure and robust environment by implementing features such as memory protection and a clear distinction between user and kernel modes, making it a more suitable solution for embedded systems and IoT devices with security and stability concerns.

# Key Features
* Thread management: Noza supports preemptive multithread for SMP core, APIs including thread creation, termination, and synchronization through system calls, enabling efficient multi-threading.
* Inter-process communication (IPC): Noza facilitates communication between threads using message passing, ensuring seamless and synchronized execution of tasks.

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
- 預設 per-process heap 採用 `tinyalloc`，也可以在 CMake 開啟 `-DNOZA_PROCESS_USE_TLSF=ON` 切換到 TLSF（Two-Level Segregated Fit） allocator，以獲得較穩定的配置延遲。兩種 allocator 都共享相同 API，僅影響記憶體管理策略。
- Application 若需要 POSIX 風格名稱，可以在自己的 header 中選擇 `#define open noza_open` 等別名，但預設請直接使用 `noza_*` 版本，確保呼叫會走到 Noza 的服務層而不是 Pico SDK 的預設 stub。
- 若完全停用 newlib，需自行提供啟動碼、`__aeabi_*` runtime 及 syscall stub，並重新調整 Pico SDK 的 link 過程。本專案暫時維持「平台層 newlib + process 層 Noza libc」的分層方式，以便同時享有硬體支援與 process 隔離。

# System Calls
* `noza_thread_join(uint32_t thread_id)`: Waits for the specified thread to terminate before continuing. 
* `noza_thread_sleep(uint32_t ms)`: Puts the current thread to sleep for the specified number of milliseconds. 
* `noza_thread_create(void (*entry)(void *), void *param, uint32_t priority)`: Creates a new thread with the given entry function, parameter, and priority. 
* `noza_thread_change_priority(uint32_t thread_id, uint32_t priority)`: Changes the priority of the specified thread. 
* `noza_thread_terminate()`: Terminates the current thread.
* 'noza_thread_self(uint32_t *id)': Get running thread ID
* `noza_recv(noza_msg_t *msg)`: Receives a message from another process, blocking until a message is received. 
* `noza_reply(noza_msg_t *msg)`: Replies to a message received from another process. 
* `noza_call(noza_msg_t *msg)`: Sends a message to another process and waits for a reply, blocking until the reply is received. 
* `noza_nonblock_call(uint32_t pid, noza_msg_t *msg)`: Sends a message to another process without waiting for a reply, returning immediately. 
* `noza_nonblock_recv(uint32_t pid, noza_msg_t *msg)`: Receives a message from another process without blocking, returning immediately.

All these functions make use of the `noza_syscall` function, which is a thin wrapper around the actual system call mechanism. This function takes four arguments (r0, r1, r2, r3) and passes them to the kernel. Each system call has a unique identifier, such as `NSC_YIELD`, `NSC_THREAD_JOIN`, etc., which is used as the first argument to `noza_syscall`. The other arguments depend on the specific system call being made.

# Build
To build the Noza microkernel project using the pico_sdk and cmake, follow the steps below. These instructions assume that you have already cloned the project from GitHub to your local machine.
- To build the Noza microkernel project using the pico_sdk and cmake, follow the steps below. These instructions assume that you have already cloned the project from GitHub to your local machine. 
1. **Install dependencies** : Ensure that you have the following software installed on your system:
- CMake (version 3.12 or later)
- GCC (with ARM cross-compilation support)
- GNU Make
2. **Set up the Raspberry Pi Pico SDK** : If you haven't already set up the Raspberry Pi Pico SDK (pico_sdk), follow the official guide for your operating system
3. **Clone the Noza repository** : If you haven't already cloned the Noza microkernel project from GitHub, do so now by running the following command:
```bash
git clone https://github.com/hkt999/noza_os.git
``` 
4. **Navigate to the project directory** : Change to the Noza project directory:

```bash
cd noza_os
``` 
5. **Create a build directory** : Make a new directory to store the build files:

```bash
mkdir build
``` 
6. **Navigate to the build directory** : Change to the build directory:

```bash
cd build
``` 
7. **Generate build files** : Run CMake to generate the build files, specifying the path to the pico_sdk:

```php
cmake -DPICO_SDK_PATH=<path_to_pico_sdk> ..
```

Replace `<path_to_pico_sdk>` with the actual path to your pico_sdk directory. 
8. **Build the project** : Compile the Noza microkernel by running:

```go
make
``` 
9. **Upload the binary to your Raspberry Pi Pico** : Follow the Raspberry Pi Pico documentation to upload the generated binary (e.g., `noza.uf2`) to your Raspberry Pi Pico.

After completing these steps, you should have successfully built and uploaded the Noza microkernel to your Raspberry Pi Pico.

# Future works
1. POSIX Style API
2. Virtual file system interface

