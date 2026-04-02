# milling_basic

`milling_basic` is a Raspberry Pi Pico firmware project for the CC2511 2026 milling-machine board. 

The project includes a header file `mmhal.h`, which declares a hardware access layer (HAL) which
should be used in the project.

You should implement the HAL in mmhal.c, following the hints in the TODOs.

## Build

This project uses the Raspberry Pi Pico SDK and standard CMake tooling.

Typical workflow:

```powershell
mkdir build
cd build
cmake ..
cmake --build .
```

Expected build inputs:

- A working Pico SDK setup available to CMake.

The build target is named `milling_basic`.

