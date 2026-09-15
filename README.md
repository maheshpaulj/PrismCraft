# 🔺 PrismCraft

<div align="center">

![Version](https://img.shields.io/badge/version-v0.1.0-blue.svg)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

**A voxel sandbox game built entirely from scratch on an equilateral triangular prism honeycomb lattice.**

[Features](#-features) • [Geometric Mathematics](#-geometric-mathematics) • [Controls](#-controls--keybindings) • [Building & Running](#-building--running) • [Roadmap & TODOs](#-roadmap--todos) • [Architecture](#-architecture)

</div>

---

## 📖 Overview

**PrismCraft** departs fundamentally from traditional cubic voxel engines. Rather than dividing space into cubes, every unit column is tessellated into pairs of **equilateral triangular prisms**. This geometric shift produces natural 60° diagonal slopes, sharp architectural ridges, and hexagonal structural formations impossible in standard grid-based voxel games.

Built in modern **C++20** with **Vulkan 1.3**, PrismCraft incorporates dynamic rendering (zero legacy render passes), cascaded shadow maps, atmospheric volumetric fog, screen-space reflections, temporal super-resolution upscaling, a 3D audio engine, and a data-driven 122-block catalog.

---

## ✨ Features

### 📐 Geometry & Voxel Engine
* **Equilateral Triangular Prism Honeycomb**: Space is parameterized by `(x, y, z, s)`, where `s ∈ {0, 1}` denotes opposing triangle orientations (pointing North/Up vs. South/Down).
* **5-Neighbor Adjacency Topology**: Every cell possesses exactly 5 neighbors: Top, Bottom, Base wall, Left slanted wall, and Right slanted wall.
* **Unified Normal Triangle Texture Mapping**: Standardized `(u, v)` mapping ensures seamless texture alignment across triangles of opposing chiralities without inverted upside-down rendering.
* **Cellular Water & Gravity Simulator**: Dynamic fluid simulation with directional surface flow vectors, underwater slopes, and gravity-affected falling sand and gravel.
* **Multithreaded Chunk Management**: Concurrent chunk generation and greedy meshing powered by a worker thread pool and high-RAM chunk cache.

### 🎨 AAA Vulkan 1.3 Graphics Pipeline
* **Dynamic Rendering**: Employs `VK_KHR_dynamic_rendering` and `VK_KHR_synchronization2` for streamlined pipeline transitions without legacy `VkRenderPass` or `VkFramebuffer` overhead.
* **Cascaded Shadow Mapping (CSM)**: 2-cascade directional shadow mapping with PCF (Percentage-Closer Filtering) for sharp foreground silhouettes and stable distant terrain shadows.
* **Atmospheric Aerial Perspective**: Physical Henyey-Greenstein Mie forward scattering, solar limb darkening, and golden-hour sunset transitions.
* **Volumetric Oceanic Water**:
  * Spectral Beer-Lambert wavelength absorption (selective red/green decay leaving deep oceanic cyan/blue).
  * Animated 3D fluid caustic light webs and volumetric sun shafts (god rays).
  * Screen-Space Reflections (SSR) displaced by bidirectional Gerstner swells.
  * Snell's window total internal reflection effect when submerged.
* **Temporal Super Resolution (TSR / FSR)**:
  * Halton `(2, 3)` subpixel camera jitter.
  * History accumulation buffer with Catmull-Rom bicubic reconstruction and YCoCg neighborhood variance bounding.
  * Robust Contrast Adaptive Sharpening (RCAS) post-pass.
  * 4 performance presets: Ultra Quality (77%), Quality (67%), Balanced (59%), Performance (50%).
* **Contact Ambient Occlusion & Decals**:
  * 6-sector hexagonal bisector ray AO tailored for equilateral triangular prism vertices.
  * Multiplicative block damage cracking (`BlendMode::Multiply`) preserving block textures beneath fractures.
  * Outlined block selector with 3D prism wireframes.

### ⛏️ Content & Gameplay Mechanics
* **122 Data-Driven Blocks**: Defined in [`assets/blocks.json`](assets/blocks.json) with custom hardness, tool requirements, drop tables, and per-face textures.
* **Custom 3D Models in Triangular Geometry**:
  * **Beds**: Red quilt mattresses with authentic Minecraft bed wood side skirts, dark foot caps, and plank undersides.
  * **Lanterns & Torches**: Hanging and standing lanterns, wall-mounted torches with realistic ~21° inward slant.
  * **Doors & Trapdoors**: Multi-species interactive wooden and iron doors/trapdoors with opening and closing animations.
  * **Cacti**: Seamless 14×14 inset model with sealed corner geometry.
  * **Cake**: Partial-height tiered cake blocks.
* **Crafting System**: 3×3 Crafting Table and 2×2 player inventory grid supporting full recipe trees for wooden, stone, and metal tools.
* **World Persistence**: Binary chunk serializer and world save manager for saving and loading terrain modifications.
* **Spatial 3D Audio**: OpenAL-based audio subsystem with surface-dependent footstep sound effects, block breaking/placement audio, and ambient music tracks.

---

## 📐 Geometric Mathematics

In PrismCraft's coordinate system, space in the horizontal `(X, Z)` plane is discretized into alternating equilateral triangles with side length `L = 1.0` and row height `H = sqrt(3)/2 ≈ 0.8660254`.

Each `(x, z)` column is divided into two prisms denoted by sub-index `s`:

```
Row Parity = (z mod 2)
Row Offset = (Row Parity == 1) ? 0.5 : 0.0

s = 0 (North / UP Triangle):
  v0 = (x + Offset,        z * H)
  v1 = (x + Offset + 1.0,  z * H)
  v2 = (x + Offset + 0.5,  (z + 1) * H)

s = 1 (South / DOWN Triangle):
  v0 = (x + Offset + 1.0,  z * H)
  v1 = (x + Offset + 1.5,  (z + 1) * H)
  v2 = (x + Offset + 0.5,  (z + 1) * H)
```

```
       Base Wall (Neighbor 2)
       v0 ------------- v1
         \             /
          \   s = 0   /
Left Wall  \  (North)/  Right Wall
(Nbr 3)     \       /   (Nbr 4)
             \     /
                v2
```

Every prism connects to **5 neighbors**:
1. **Face 0**: Top `(y + 1, s)`
2. **Face 1**: Bottom `(y - 1, s)`
3. **Face 2**: Base Wall (horizontal neighbor across the flat edge)
4. **Face 3**: Left Slanted Wall (horizontal neighbor across the left 60° edge)
5. **Face 4**: Right Slanted Wall (horizontal neighbor across the right 60° edge)

---

## 🎮 Controls & Keybindings

| Key / Input | Action |
|:---|:---|
| **W / A / S / D** | Move Forward / Left / Backward / Right |
| **Space** | Jump / Swim Upward |
| **Left Shift** | Sneak / Descend |
| **Left Ctrl** | Toggle Sprint |
| **Left Mouse Button (LMB)** | Mine / Break targeted triangular prism block |
| **Right Mouse Button (RMB)** | Place block / Interact (Doors, Trapdoors, Beds, Crafting) |
| **Middle Mouse Button (MMB)**| Pick block into active hotbar slot |
| **1 – 9 / Mouse Wheel** | Select active hotbar slot |
| **E** | Open / Close Player Inventory & 2×2 Crafting |
| **Q** | Drop active held item entity |
| **F3** | Toggle Debug HUD (Coordinates, Chunk, Facing, FPS) |
| **F11** | Toggle Fullscreen Mode |
| **Escape (Esc)** | Pause Menu / Settings / Close open menus |

---

## 🛠️ Building & Running

### Prerequisites
1. **Operating System**: Windows 10 or Windows 11 (64-bit).
2. **Compiler**: Visual Studio 2022 (MSVC v143+) with **Desktop development with C++** and C++20 standard support.
3. **Vulkan SDK**: [LunarG Vulkan SDK](https://vulkan.lunarg.com/) version **1.3.296+** installed with `glslc` added to PATH.
4. **CMake** (3.24+) or **MSBuild**.

### Build via MSBuild (Recommended)
```powershell
# 1. Compile Shaders
.\compile_shaders.bat

# 2. Build Release x64
& "D:\Softwares\Visual Studio\MSBuild\Current\Bin\MSBuild.exe" PrismCraft.vcxproj /p:Configuration=Release /p:Platform=x64 /m

# 3. Launch
.\x64\Release\PrismCraft.exe
```

### Standalone Export & Installer
To generate a portable zero-dependency ZIP archive and Inno Setup installer:
```powershell
# Run the automated packaging script
.\package_game.ps1 -Version "0.1.0"

# Output will be located in:
# - dist/PrismCraft/                      (Self-contained folder with all runtime DLLs)
# - dist/PrismCraft_v0.1.0_Windows_x64.zip (Standalone portable archive)
```

---

## 🗺️ Roadmap & TODOs

See [ROADMAP.md](ROADMAP.md) for full phase details and technical design notes.

### 🎯 v0.2.0 (Mob AI, Combat & Equipment)
- [ ] **Triangular Graph A\* Pathfinding**: Navigation mesh traversal adapted for the 5-neighbor triangular honeycomb lattice.
- [ ] **Entity Component System (ECS)**: Lightweight data-oriented entity management for mobs, projectiles, and dropped items.
- [ ] **Armor & Equipment System**:
  - [ ] Leather, Iron, Gold, Diamond, Netherite armor pieces (Helmet, Chestplate, Leggings, Boots).
  - [ ] Triangular armor bar HUD indicators and authentic damage reduction formulas.
- [ ] **3D Wooden Boats**: Buoyant fluid physics on cellular water, dual-paddle rowing controls, passenger seating.
- [ ] **Passive Animals**: Pig (saddle ride), Cow (milking, beef/leather), Sheep (colored wool shearing), Chicken (egg laying).
- [ ] **Hostile Mobs**: Zombie (daylight burning, door siege), Skeleton (parabolic archery), Creeper (triangular crater detonation), Spider (slanted wall climbing).
- [ ] **Combat Mechanics**: Weapon damage tiers, knockback impulse, critical hits, hurt sounds, and red flash damage overlays.

### 🌐 v0.3.0 (Multiplayer & Networking)
- [ ] **Dedicated Client-Server Architecture**: Standalone headless server binary.
- [ ] **Reliable UDP Protocol**: ENet or custom UDP transport with packet sequencing.
- [ ] **Delta-Compressed Chunk Streaming**: RLE/zstd network serialization for modified chunks.
- [ ] **Client-Side Prediction**: Movement interpolation, entity reconciliation, and server-authoritative block verification.
- [ ] **Multiplayer Chat & Player Tab List**: In-game text messaging, whisper commands, and player skins.

### 🌌 v0.4.0 (Dimensions & Realm Travel)
- [ ] **The Nether Realm**:
  - [ ] Dual-ceiling 3D cave generation (Netherrack, Soul Sand Valleys, Basalt Deltas, Crimson/Warped Forests).
  - [ ] Triangular Obsidian Nether Portals with custom swirl screen shaders.
  - [ ] Lava oceans with custom visceral fluid shaders and Nether Fortresses.
- [ ] **The End Dimension**:
  - [ ] Central floating island generation with Obsidian Pillars.
  - [ ] Void death plane and End Gateway portals.
  - [ ] Ender Dragon boss encounter with animated spline models and healing End Crystals.

### ⚡ v0.5.0 (Redstone & Triangular Logic Circuitry)
- [ ] **Triangular Redstone Signal Propagation**: Power distribution rules across 5 adjacent neighbors.
- [ ] **Components**:
  - [ ] Redstone Dust on triangular prism tops and slanted walls.
  - [ ] Redstone Torches, Levers, Buttons, and Pressure Plates.
  - [ ] Repeaters (delay adjustment) and Comparators.
  - [ ] Normal and Sticky Triangular Pistons with block pushing constraints.

### 🚀 Advanced Performance, Shading & World Gen Backlog
- [ ] **Native AMD FidelityFX Super Resolution (FSR 2/3) SDK**: Official C++ SDK integration with motion vectors, depth reprojection, and reactive masks.
- [ ] **Advanced 3D Terrain & Caves**: Multi-octave 3D Simplex overhangs, cheese caverns, spaghetti tunnels, and continuous valley-carving rivers.
- [ ] **Procedural Structures**: Multi-building triangular villages with roads, farms, and blacksmiths; desert temples with secret pressure-plate TNT traps and loot crypts.
- [ ] **Authentic Triangular UI Theme**: Triangular health hearts, triangular hunger drumsticks, hexagonal/triangular hotbar slots, and isometric UI panels.
- [ ] **Faithful Flowing Triangular Textures**: 60° equilateral texture mapping preserving natural honeycomb symmetries across walls and floors.
- [ ] **GPU Indirect Drawing & Multithreaded Mesher**: `vkCmdDrawIndexedIndirect` with GPU-side frustum culling and parallel greedy meshing.

### 📦 v1.0.0 (Extensibility, Modding & Optimization)
- [ ] **Lua/C++ Scripting API**: Dynamic block registration, custom crafting recipes, and event hooks.
- [ ] **Resource Pack Engine**: Hot-reloading custom textures, audio packs, and block definitions without recompiling.
- [ ] **Vulkan Ray Tracing (VK_KHR_ray_tracing)**: Hardware-accelerated RT shadows and global illumination for compatible GPUs.
- [ ] **Cross-Platform Support**: Linux (X11/Wayland) port with Clang / GCC toolchain.

---

## 🏛️ Architecture

```
PrismCraft/
├── assets/
│   ├── blocks.json               # Complete catalog of 122 data-driven blocks
│   ├── music/                    # Ambient background soundtrack files
│   ├── sounds/                   # Footsteps, digging, breaking, UI sound effects
│   ├── shaders/                  # GLSL 450 vertex, fragment, and compute shaders
│   └── textures/                 # 1024x1024 atlas source textures and entity sheets
├── src/
│   ├── audio/                    # OpenAL audio device, source, and sound manager
│   ├── core/                     # Window (GLFW), Input, Timer, ConfigManager, ThreadPool
│   ├── data/                     # BlockRegistry, SimpleJson parser
│   ├── game/                     # Crafting recipes, inventory management
│   ├── player/                   # Camera (view/proj, frustum), Player physics, Raycast DDA
│   ├── renderer/                 # Vulkan pipeline managers, TSR upscaler, TextureStitcher
│   ├── rhi/                      # VulkanContext, Swapchain, CommandQueue, Pipeline, Buffer
│   ├── ui/                       # Bitmap font renderer, UI overlay, pause & main menus
│   └── world/                    # Chunk, ChunkMesher, Coordinates, TerrainGen, WaterSim
├── installer/                    # Inno Setup installation scripts
├── dist/                         # Distribution and packaged releases (.gitignore)
├── compile_shaders.bat           # Automated SPIR-V shader compiler script
├── package_game.ps1              # Full standalone packager script
└── PrismCraft.vcxproj            # Visual Studio 2022 project configuration
```

---

## 📜 License

PrismCraft is distributed under the **MIT License**. See `LICENSE` for details. Game audio and original textures are sourced under permissive licenses (CC0 / Public Domain / Royalty-Free).\n