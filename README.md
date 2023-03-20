# Noza OS
Noza is a lightweight, minimalist microkernel inspired by the mythical figure Noza (哪吒) from Eastern culture, known for being small, agile, and incredibly fast. In the legends, Noza is said to ride the Wind Fire Wheels (風火輪), which allow for swift and effortless movement. The Noza microkernel embodies these qualities, providing a unique and efficient solution for embedded systems and IoT devices. By focusing on core functionality like thread management and IPC, Noza offers a fast and efficient operating system tailored for embedded systems and IoT devices.

# Motivation
The design of Noza is partly inspired by the limitations of some real-time operating systems like FreeRTOS, which do not enforce strict separation between user mode and kernel mode. Noza aims to provide a more secure and robust environment by implementing features such as memory protection and a clear distinction between user and kernel modes, making it a more suitable solution for embedded systems and IoT devices with security and stability concerns.

# Key Features
* Thread management: Noza supports thread creation, termination, and synchronization through system calls, enabling efficient multi-threading.
* Inter-process communication (IPC): Noza facilitates communication between threads using message passing, ensuring seamless and synchronized execution of tasks.

# System Calls
* `noza_thread_yield()`: Yields the current thread's execution, allowing other threads to run. 
* `noza_thread_join(uint32_t thread_id)`: Waits for the specified thread to terminate before continuing. 
* `noza_thread_sleep(uint32_t ms)`: Puts the current thread to sleep for the specified number of milliseconds. 
* `noza_thread_create(void (*entry)(void *), void *param, uint32_t priority)`: Creates a new thread with the given entry function, parameter, and priority. 
* `noza_thread_change_priority(uint32_t thread_id, uint32_t priority)`: Changes the priority of the specified thread. 
* `noza_thread_terminate()`: Terminates the current thread. 
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
1. Utilize the ARM Cortex-M0 MPU to protect kernel stack and data structures.
2. Create a simple console for user interaction and system management.
3. Implement a memory management service to provide heap and stack memory for newly created threads.
4. Develop a virtual file system service to facilitate file operations.
5. Implement an ELF object loader to dynamically load applications from the virtual file system.


