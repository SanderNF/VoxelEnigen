# Minecraft voxel enigen
![License: All Rights Reserved](https://img.shields.io/badge/License-All%20Rights%20Reserved-red.svg)

![OpenGL](https://img.shields.io/badge/OpenGL-%23FFFFFF.svg?style=for-the-badge&logo=opengl)
![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake)
![MSYS2](https://img.shields.io/badge/MSYS2-262D3A?style=for-the-badge&logo=msys2)
![Make](https://img.shields.io/badge/Make-000000?style=for-the-badge)


A voxel-based engine built from scratch in C++ and OpenGL. Features procedurally generated infinite worlds with Perlin noise terrain, biome systems, tree generation, optimized chunk rendering. Includes a first-person player controller with physics, collision detection, block placement/destruction, and a texture atlas system for efficient rendering.

## features

## screenshots
![Screenshot.png](Screenshot.png)
## build instructions
### Prerequisites
you need a c++ compiler.
* MSYS2 MINGW64 (windows)
* CMake (build system)
* OpenGL Support Libraries: GLFW, GLEW GLM

### installation
windows (MSYS2)
1. install msys2 from https://www.msys2.org/
2. open msys2 and install the packages:
````
pacman -S mingw-w64-x86_64-glew mingw-w64-x86_64-glfw mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-toolchain
````
3. close the repository:
````bash
git clone https://github.com/mathiassol/VoxelEnigen.git
cd VoxelEnigen
````

4. clone imgui repository into src:
```` bash
cd src
git clone https://github.com/ocornut/imgui.git
````
5. build the project:
```` bash
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
````
6. Handle DLL dependencies:
   If the executable doesn't run, copy these DLLs from your MSYS2 installation's /mingw64/bin folder to your /build folder:




## dev note
 0-6 is FRONT, BACK, LEFT, RIGHT, BOTTOM, TOP