Siphon
======

Utility to send files between two machines over a direct TCP connection, written in C.
One peer listens, the other connects; each side can both send and receive files in a single session.

Usage
-----

On the listening side:

    siphon --listen --directory /path/to/save/

On the connecting side:

    siphon --connect <host> --file path/to/file1 --file path/to/file2

Options:

    -l, --listen           Listen for an incoming connection.
    -c, --connect <host>   Connect to a listening host.
    -p, --port <n>         TCP port (default: 3214).
    -f, --file <path>      File to send. Repeat for multiple files.
    -d, --directory <dir>  Where to store received files (trailing slash required).
    -b, --buffer <bytes>   Transfer buffer size (default: 5120).
    -v, --version          Print version.
    -h, --help             Show help.

Build
-----

    cmake -B build && cmake --build build

Screenshot
----------

![image](https://raw.github.com/solkin/siphon-cli/master/art/main.png)
