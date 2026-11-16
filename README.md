# LDRE
A highly work in progress demo editor for classic Doom games, used primarily for creating tool-assisted speedruns of the games.

## Linux support

* Linux support is work in progress and needs more testing, but you should be able to build a demo.
* Brute force can be faster on Linux compared to Windows due to bad thread scheduling on Windows.
* Linux uses a software-rendered GUI instead of the OpenGL GUI available in Windows, it's not a big issue and will be looked into later.
* Binaries are currently unavailable for Linux until I set up AppImages so you'll need to compile it yourself.

## Compiling instructions

* Windows (Visual Studio 2026)

1. Install LLVM, clang-cl, vcpkg and CMake within Visual Studio Installer.
2. Run the batch file included in _build_vs
3. Choose the build target you want (optimized is ideal) and enable Link-Time Optimization if you want faster brute forcing.
4. The output will be in source/build/windows-clang/xdre/Release.

* Linux/MSYS2 instructions

Install the following dependencies if on Windows (while using MSYS2 UCRT64):

```
mingw-w64-ucrt-x86_64-git
mingw-w64-ucrt-x86_64-gcc
mingw-w64-ucrt-x86_64-ninja
mingw-w64-ucrt-x86_64-libmad
mingw-w64-ucrt-x86_64-libzip
mingw-w64-ucrt-x86_64-libxmp
mingw-w64-ucrt-x86_64-portmidi
mingw-w64-ucrt-x86_64-SDL2_image
mingw-w64-ucrt-x86_64-SDL2_mixer
mingw-w64-ucrt-x86_64-SDL2_net
mingw-w64-ucrt-x86_64-cmake
mingw-w64-ucrt-x86_64-imagemagick
```

Install the following dependencies if on Linux:

```
base
base-devel
git
gcc
fluidsynth
libmad
libxmp
libzip
portmidi
SDL2_image
SDL2_mixer
SDL2_net
cmake
imagemagick
ninja
```

Clone the project:

```
git clone https://github.com/ooferdww/LDRE.git
```

Enter the directory:

```
cd ./LDRE
```

Create build directory:

```
mkdir -p build
```

Configure with CMake:

```
cmake -B build -S . --fresh -DCMAKE_BUILD_TYPE=Release
```

Build project:

```
ninja -C build
```

a build script for Linux will be created soon

## XDRE heritage

This project uses XDRE as a basis, credit and thanks to authors:

* RamboBones
* xepop
* vita
* almostmatt1

## dsda-doom heritage
This project uses dsda-doom for game logic, credit and thanks to authors:
* fabiangreffrath
* rfomin
* Pedro-Beirao
* andrikpowell
* elf-alchemist
* kraflab
* Anyone else who contributed to dsda-doom
