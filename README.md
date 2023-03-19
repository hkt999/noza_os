# Noza OS
Noza is a lightweight, minimalist microkernel inspired by the mythical figure Noza (哪吒) from Eastern culture, known for being small, agile, and incredibly fast. In the legends, Noza is said to ride the Wind Fire Wheels (風火輪), which allow for swift and effortless movement. The Noza microkernel embodies these qualities, providing a unique and efficient solution for embedded systems and IoT devices. By focusing on core functionality like thread management and IPC, Noza offers a fast and efficient operating system tailored for embedded systems and IoT devices.

# Motivation
The design of Noza is partly inspired by the limitations of some real-time operating systems like FreeRTOS, which do not enforce strict separation between user mode and kernel mode. Noza aims to provide a more secure and robust environment by implementing features such as memory protection and a clear distinction between user and kernel modes, making it a more suitable solution for embedded systems and IoT devices with security and stability concerns.

# Key Features
* Thread management: Noza supports thread creation, termination, and synchronization through system calls, enabling efficient multi-threading.
* Inter-process communication (IPC): Noza facilitates communication between threads using message passing, ensuring seamless and synchronized execution of tasks.

# Build
To build the Noza microkernel project using the pico_sdk and cmake, follow the steps below. These instructions assume that you have already cloned the project from GitHub to your local machine.
- To build the Noza microkernel project using the pico_sdk and cmake, follow the steps below. These instructions assume that you have already cloned the project from GitHub to your local machine. 
1. **Install dependencies** : Ensure that you have the following software installed on your system:
- CMake (version 3.12 or later)
- GCC (with ARM cross-compilation support)
- GNU Make
2. **Set up the Raspberry Pi Pico SDK** : If you haven't already set up the Raspberry Pi Pico SDK (pico_sdk), follow the official guide for your operating system: 
- [Getting Started with Raspberry Pi Pico for Linux](https://datasheets.raspberrypi.org/pico/getting-started-with-pico-linux.pdf) 
- [Getting Started with Raspberry Pi Pico for macOS](https://datasheets.raspberrypi.org/pico/getting-started-with-pico-macos.pdf) 
- [Getting Started with Raspberry Pi Pico for Windows](https://datasheets.raspberrypi.org/pico/getting-started-with-pico-windows.pdf) 
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
