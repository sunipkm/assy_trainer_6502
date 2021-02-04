# Assembly Trainer for MOS6502 using the C emulator backend
### Requirements:
1. Linux, macOS or Windows (WSL recommended, MinGW otherwise)
2. GLFW3 Library (Ubuntu: `sudo apt install libglfw3-dev`, macOS: `brew install glfw`, Google for MinGW)
3. Clone using `git clone https://github.com/sunipkmukherjee/assy_trainer/6502.git --recurse-submodules` to ensure that all submodules are cloned.

Based on [Dear ImGui](https://github.com/ocornut/imgui).

### Run:
To build, run `make` in command line, then execute `./mos6502.out`.
First build takes a long time in order to build the Dear ImGui backend.

Happy testing!