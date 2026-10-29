# LDRE
A highly work in progress demo editor for classic Doom games, used primarily for creating tool-assisted speedruns of the games.

## Build instructions

Install the following dependencies if on Windows (use MSYS2 UCRT64):

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
cmake -B build -S . --fresh
```

Build whole project:

```
ninja -C build
```

Alternatively only build LDRE:

```
ninja -C build xdre
```

Alternatively only build dsda-doom:

```
ninja -C build dsda-doom
```

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
