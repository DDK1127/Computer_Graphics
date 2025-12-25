# HW2 — Textured Mesh Viewer

This project is a simple OpenGL 3.3 viewer that loads and displays textured 3D meshes (`.obj` + `.mtl` + `.jpg`).

It uses **C++17**, **GLFW**, **GLAD**, **GLM**, **TinyOBJLoader**, and **stb_image**.
Two executable programs will be built:
- `tiger_viewer` — loads `assets/tiger.obj`
- `buddha_viewer` — loads `assets/buddha.obj`

---

## 🧰 Folder Structure
> Texture a mesh/
> ├─ src/
> │ ├─ main.cpp
> │ ├─ camera.h
> │ └─ shaders/
> │ ├─ mesh.vert
> │ └─ mesh.frag
> ├─ external/ # glad, tinyobjloader, stb_image
> ├─ assets/ # model .obj, .mtl, and textures
> └─ CMakeLists.txt

### macOS
```bash=
brew install glfw glm
mkdir build && cd build
cmake ..
make -j
```

### Linux (Ubuntu / Debian)
```bash=
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libglm-dev
mkdir build && cd build
cmake ..
make -j
```

### Windows (PowerShell + vcpkg)
- Install vcpkg
- Install dependencies: `vcpkg install glfw3 glm`
```bash=
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build . -j
```

### Run 
After build, two executables will appear in build/:

> build/
>  ├─ tiger_viewer
>  └─ buddha_viewer

Each executable automatically copies: assets/, shaders/ to its folder.

Run inside build/: ./tiger_viewer && ./buddha_viewer