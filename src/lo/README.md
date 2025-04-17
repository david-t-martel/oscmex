liblo
=====

liblo is a lightweight library that provides an easy to use implementation of
the OSC protocol. For more information about the OSC protocol, please see:

- [OSC at CNMAT](http://www.cnmat.berkeley.edu/OpenSoundControl/)
- [https://opensoundcontrol.stanford.edu/](https://opensoundcontrol.stanford.edu/)
- [https://en.wikipedia.org/wiki/Open_Sound_Control](https://en.wikipedia.org/wiki/Open_Sound_Control)

The official liblo homepage is here:

- [liblo homepage](http://liblo.sourceforge.net/)

liblo is portable to most UNIX systems (including OS X) and
Windows. It is released under the GNU Lesser General Public Licence
(LGPL) v2.1 or later.  See COPYING in the project root for details.

Building
--------

This project uses CMake for building. Please refer to the `BUILD_INSTRUCTIONS.md` file in the project's root directory for detailed build instructions.

Examples
--------

See the `examples` directory for example source code demonstrating liblo usage:

- `example_server.c`: Creates a liblo server in a separate thread using `lo_server_thread_start()`.
- `nonblocking_server_example.c`: Uses `select()` (or `poll()`) to wait for console input or OSC messages in a single thread.
- `example_client.c`: Sends OSC messages to a server.
- `cpp_example.cpp`: Demonstrates usage of the C++ wrapper (`lo_cpp.h`).

These examples can be built by enabling the `BUILD_LO_EXAMPLES` option in CMake (see `BUILD_INSTRUCTIONS.md`).

Debugging
---------

To build with debugging symbols, configure CMake with the Debug build type:

    cmake .. -DCMAKE_BUILD_TYPE=Debug

Then build the project and use your preferred debugger (like GDB or the Visual Studio Debugger).

## IPv6 NOTICE

liblo was written to support both IPv4 and IPv6. However, IPv6 support can sometimes cause issues with applications that do not explicitly listen on IPv6 sockets (like older versions of Pd or SuperCollider). IPv6 is currently disabled by default in the CMake build configuration but can be enabled if needed by modifying the CMakeLists.txt (this is currently not exposed as a simple option).

## Poll() vs Select()

Some platforms may have both `poll()` and `select()` available for listening efficiently on multiple servers/sockets. In this case, liblo (when built via CMake) will typically use the available function detected by CMake's checks (`CheckSymbolExists`). `poll()` is generally considered more scalable. However, on some platforms (e.g., macOS), `select()` might perform better for less demanding applications. The CMake configuration automatically detects and uses the available function, preferring `poll` if found.
