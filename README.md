# Noza OS
Noza is a lightweight, minimalist microkernel inspired by the mythical figure Noza (哪吒) from Eastern culture, known for being small, agile, and incredibly fast. In the legends, Noza is said to ride the Wind Fire Wheels (風火輪), which allow for swift and effortless movement. The Noza microkernel embodies these qualities, providing a unique and efficient solution for embedded systems and IoT devices. By focusing on core functionality like thread management and IPC, Noza offers a fast and efficient operating system tailored for embedded systems and IoT devices.

# Motivation
The design of Noza is partly inspired by the limitations of some real-time operating systems like FreeRTOS, which do not enforce strict separation between user mode and kernel mode. Noza aims to provide a more secure and robust environment by implementing features such as memory protection and a clear distinction between user and kernel modes, making it a more suitable solution for embedded systems and IoT devices with security and stability concerns.

# Key Features:
* Thread management: Noza supports thread creation, termination, and synchronization through system calls, enabling efficient multi-threading.
* Inter-process communication (IPC): Noza facilitates communication between threads using message passing, ensuring seamless and synchronized execution of tasks.

