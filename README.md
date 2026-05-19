Siphon
======

Utility to send files between two machines over a direct TCP connection, written in C.
One peer listens, the other connects; each side can both send and receive files in a single session.
Transfers are verified with MD5, preserve the executable bit, and can walk whole directory trees.

Usage
-----

On the listening side:

    siphon --listen --directory /path/to/save

On the connecting side:

    siphon --connect <host> --file path/to/file --file path/to/dir

Each `--file` argument can be either a regular file or a directory. Directories are
walked recursively; the root directory's own name is dropped and the inner structure
is recreated under the receiver's `--directory`.

Options:

    -l, --listen           Listen for an incoming connection.
    -c, --connect <host>   Connect to a listening host.
    -p, --port <n>         TCP port (default: 3214).
    -f, --file <path>      File or directory to send. Repeat for multiple items.
    -d, --directory <dir>  Where to store received files.
    -b, --buffer <bytes>   Transfer buffer size (default: 5120).
    -L, --legacy           Send using the v1 protocol (no MD5, no path) for
                           compatibility with pre-v2 peers.
    -v, --version          Print version.
    -h, --help             Show help.

Build
-----

    cmake -B build && cmake --build build

Screenshot
----------

![image](https://raw.github.com/solkin/siphon-cli/master/art/main.png)
