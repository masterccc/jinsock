# Socket Injector Shell

A command-line tool to interact with TCP sockets opened by other processes on the same Linux machine.  
Allows you to search, select, and duplicate existing sockets (via `pidfd_getfd`), send data or files through them, and receive data with a configurable timeout.

## Features

- List TCP sockets opened by processes, optionally filtered by PID or process name.
- Display remote IP address and port for each socket.
- Select a socket from search results to interact with.
- Send arbitrary strings or entire files into the selected socket.
- Receive data from the socket with a timeout and optionally save to a file.
- Configure receive timeout.
- Simple interactive shell interface with helpful commands.

## Requirements

- Linux kernel 5.6 or newer (for `pidfd_open` and `pidfd_getfd` syscalls)
- Root privileges to access other processes' file descriptors
- GCC to compile

## Compilation

```bash
gcc -o socket_injector socket_injector.c
````

## Usage

Run the program as root:

```bash
sudo ./socket_injector
```

### Commands

* `help`
  Show help with available commands.

* `search [pattern]`
  List all sockets. If a pattern is given, filter by PID or process name containing the pattern.

* `select <index>`
  Select a socket from the last search results by its index.

* `send <string>`
  Send a string to the selected socket.

* `sendf <file>`
  Send the contents of a file to the selected socket.

* `rec [file]`
  Receive data from the socket with a timeout (default 5 seconds).
  If `file` is provided, save received data to it, otherwise print to stdout.
  Displays the number of bytes received after completion.

* `timeout <seconds>`
  Set the receive timeout duration (in seconds).

* `quit`
  Exit the program.

### Example session

```
> search ssh
Found 2 socket(s):
[0] PID=1234 (sshd) FD=5 -> 192.168.1.10:22
[1] PID=5678 (ssh) FD=4 -> 192.168.1.11:22

> select 0
Selected entry [0]

> send Hello World
Data sent: 11 bytes

> rec
Received 34 bytes

> quit
Bye.
```

## Notes

* This tool requires root access to open file descriptors from other processes.
* The program duplicates the file descriptor using Linux `pidfd_getfd` syscall to reuse the same socket.
* Receiving data uses `select()` with a configurable timeout to wait for incoming data.
* Tested on Linux kernel 5.6+.

## License

MIT License

---

Feel free to contribute or report issues.

```

Let me know if you want me to add anything else!
```
