What Implemented so far by myself;

* Choose Target section
* Passing empty lines in .lldb files
* Updated `lib` versions
* Line highligt after every step-instruction
* Track cursor, and set scroll to corresponding line (currently doesnt work quite right)
* Step-out stack frame
* Added `--loglevel` option
* Updated `threads` panel
* Various bugfixes
* Updated from OpenGL2 to OpenGL3
* ***HiDPI*** support!
* Threads/Stack Trace tab adjustable

---

![](https://github.com/yigithanyigit/lldbg/workflows/build-linux/badge.svg) ![](https://github.com/yigithanyigit/lldbg/workflows/build-macos/badge.svg)

---
This is an alpha-stage native GUI for lldb which is currently about 60% usable.
Right now you probably shouldn't attempt to use it unless you want to contribute to the development in some way, either directly or by submitting issues.
The basic goal is to provide vim/emacs users on linux/macOS with a lightweight, easy-to-compile, easy-to-use likeness of what you would see in a full-featured IDE debugger interface.

Primary goals are:
* open/close and respond to user input instantly, 100% of the time
* function intuitively so that 'using lldbg' is not a skill you have to learn
* require no outside configuration files/setup
* launch with the same command line options as lldb itself

## Build

```
# Tell cmake to use clang as the compiler
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

cd lldbg
mkdir build
cd build
cmake ..
cmake --build . --parallel N (where N = # of CPU cores to use)
```

![alt text](https://raw.githubusercontent.com/zmeadows/lldbg/master/screenshot.png)
