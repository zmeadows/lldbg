![](https://github.com/zmeadows/lldbg/workflows/build-linux/badge.svg) ![](https://github.com/zmeadows/lldbg/workflows/build-macos/badge.svg)

This is an alpha-stage native GUI for lldb. It is currently about 50% down the path towards usable.
The basic goal is to provide vim/emacs users on linux/macOS with a lightweight, easy-to-compile, easy-to-use likeness of what you would see in a full-featured IDE debugger interface.

Primary goals are:
* open/close and respond to user input instantly, 100% of the time
* function intuitively so that 'using lldbg' is not a skill you have to learn
* require no outside configuration files/setup
* launch with the same command line options as lldb itself

## Build

```
cd lldbg
mkdir build
cd build
cmake ..
cmake --build . --parallel N (where N = # of CPU cores to use)
```

![alt text](https://raw.githubusercontent.com/zmeadows/lldbg/master/screenshot.png)
