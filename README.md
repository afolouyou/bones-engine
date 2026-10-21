# Bones Engine

Bones Engine is a cross-platform game engine based on GameObjects and Components for Windows, Linux and PlayStation 3.

It is a continuation of [Xenity Engine](https://github.com/Fewnity/Xenity-Engine), focused on a Zig-based toolchain, a redesigned renderer with Vulkan, and a new physics layer.

**Current version: 0.01**

## Status

| Status | Platform |
|-|-|
| ✔️ | Linux (primary development build) |
| 🚧 | Windows |
| ✔️ | PlayStation 3 (RSX renderer, Docker/PSL1GHT) |

## Roadmap

- **0.01** (in progress) — Bones Engine branding, stable Zig build, C ABI (`bones/api.h`), runtime/editor split and CI. Remaining: automated scene smoke test and Windows linking.
- **0.02** (in progress) — backend capabilities and the explicit `Device`/`Buffer`/`Texture`/`Shader`/`Pipeline`/`CommandList` contract.
- **0.03** — Vulkan 1.4 backend for Linux and Windows; OpenGL remains a supported fallback.
- **0.04** — Jolt Physics replaces Bullet, with a new physics format.
- **0.05** — Stable plugin ABI (Nim scripting support is exploratory).
- **0.06** — FLTK-based editor UI.

## Compatibility

- Scenes (`.xen`) and assets from Xenity Engine projects remain readable.
- The C++ game API and project build files are not frozen and may change between releases.
- Scenes containing Bullet physics components will be rejected once Jolt is introduced; a new physics format will be required.

## Building

The build uses [Zig](https://ziglang.org/) 0.17 or newer.

On Linux, install the dependencies first (Arch):

```sh
sudo pacman -S sdl3 freetype2 assimp curl openssl
```

Then:

```sh
zig build                     # editor, installed to zig-out/bin
zig build run                 # build and run the editor
zig build -Doptimize=ReleaseFast
```

The editor and the runtime are separate builds. A runtime build needs the game source tree:

```sh
zig build -Deditor=false -Dgame-source=tests/runtime_game
./zig-out/bin/bones_runtime --version
```

`zig build` also copies the runtime assets (`engine_assets`, `public_engine_assets`, `icons`, `vscodeSample`, `plugins`) next to the executable, along with the editor font and `libassimp`.

Hosts can drive the engine through the stable C ABI in `engine/src/bones/api.h` (`bones_init`, `bones_run`, `bones_stop`, `bones_quit`). The header is installed to `zig-out/include/bones/api.h` and stays valid C.

PS3 builds still use the PSL1GHT Makefile inside Docker (`app/packaging`).

## Repository layout

- `engine/src` — engine runtime (`engine/`), editor (`editor/`), platform code (`windows/`, `ps3/`), the C ABI (`bones/`), SDK headers and unit tests.
- `app/src` — executable entry point (uses only the C ABI).
- `app/assets` — runtime assets copied next to the executable.
- `app/res` — Windows resources (icon, `.rc`).
- `app/packaging` — Dockerfile, PS3 Makefile and shader compilation script.
- `tests/runtime_game` — minimal game used to build and smoke-test the runtime without the editor.
- `third_party` — vendored libraries (headers in `third_party/<lib>`, prebuilt binaries in `third_party/libs`).
- `build.zig` — build definition.

## Using the editor

### Play a game in the editor
1. Compile the game in **Game → Compile Game**.
2. Click the play button.

### Build a game
1. Open **File → Build Settings**.
2. Select the platform.
3. Click build.

On PS3, Docker must be installed and configured (**Window → Docker Configuration**).

### Compatible files

- Images: `.png .jpg .bmp .tga`
- Audio: `.wav .mp3` (44100 Hz; use 48000 Hz on PS3)
- Meshes: `.obj .fbx`
- Fonts: `.ttf`

## How the engine works

When a project is loaded, the engine indexes every file and assigns each one a unique ID. Classes backed by files, such as `Texture`, `AudioClip` and `MeshData`, inherit from `FileReference` and keep a reference to their file.

Classes such as `Component`, `GameObject` and `Vector3` inherit from `Reflective`. This lets the inspector read and modify their fields and lets scenes serialize them without knowing the concrete type.

## License

MIT. Bones Engine is based on Xenity Engine, Copyright (c) 2022-2026 Gregory Machefer (Fewnity).

## Third-party libraries

- stb (stb_image, stb_image_resize, stb_image_write): https://github.com/nothings/stb
- drmp3 / drwav (dr_libs): https://github.com/mackron/dr_libs
- JSON for Modern C++: https://github.com/nlohmann/json
- GLM: https://github.com/g-truc/glm
- Bullet 2.82 (will be replaced by Jolt Physics)
- Assimp: https://github.com/assimp/assimp
- FreeType: https://freetype.org
- ImGui (docking branch): https://github.com/ocornut/imgui
- SDL3: https://github.com/libsdl-org/SDL
