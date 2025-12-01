# Minecraft voxel enigen
![License: All Rights Reserved](https://img.shields.io/badge/License-All%20Rights%20Reserved-red.svg)

![OpenGL](https://img.shields.io/badge/OpenGL-%23FFFFFF.svg?style=for-the-badge&logo=opengl)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake)
![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![MSYS2](https://img.shields.io/badge/MSYS2-262D3A?style=for-the-badge&logo=msys2)
![Make](https://img.shields.io/badge/Make-000000?style=for-the-badge)
![CLion](https://img.shields.io/badge/CLion-000000?style=for-the-badge&logo=clion)


a project I'm doing to learn graphics/OpenGL. 

## build
You need to have your C++ compiler set up already. There are many good tutorials on YouTube if you don't know how to do that. As I'm on Windows, I use MSYS2 MINGW64, then you install CMake and the OpenGL support libraries: GLFW, GLEW, GLM. 
````
pacman -S mingw-w64-x86_64-glew mingw-w64-x86_64-glfw
````
build and compile:
````
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
````

in many cases you will nead to have the openGL support libs in your build folder, because sometimes adding /bin to path is not enough. in that case just copy the .dll files from /bin to /build. The files you need to copy are glew32.dll, glfw3.dll, and opengl32.dll