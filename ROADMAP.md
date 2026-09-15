# 🗺️ PrismCraft Development Roadmap & Technical Specifications

> **Current Version**: `v0.1.0` (Core Sandbox Baseline)  
> **Engine**: Vulkan 1.3 / C++20 / Windows x64  
> **Target Milestone**: v1.0.0 Full Release

---

## 📋 Milestone Overview

| Phase | Target Version | Milestone Theme | Status |
|:---:|:---:|:---|:---:|
| **Phase 1** | `v0.1.0` | **Core Voxel Sandbox & Vulkan 1.3 Engine** | ✅ **Complete** |
| **Phase 2** | `v0.2.0` | **Entity Framework, Mob AI & Combat Mechanics** | 🟡 **In Planning** |
| **Phase 3** | `v0.3.0` | **Multiplayer Architecture & Client-Server Networking** | ⚪ Backlog |
| **Phase 4** | `v0.4.0` | **Alternate Dimensions (The Nether & The End)** | ⚪ Backlog |
| **Phase 5** | `v0.5.0` | **Triangular Redstone Circuitry & Automation** | ⚪ Backlog |
| **Phase 6** | `v1.0.0` | **Extensibility, Lua Modding API & Production Polish** | ⚪ Backlog |

---

## 🎯 Phase 2: Entity Framework, Mob AI & Combat (`v0.2.0`)

### 1. Triangular Graph Navigation & Pathfinding
- **5-Neighbor A\* Implementation**: Traditional cube voxel pathfinding operates on a 6- or 26-neighbor Cartesian grid. In PrismCraft, the pathfinding node graph must operate on the $(x, y, z, s)$ equilateral honeycomb lattice.
- **Node Cost Evaluation**:
  - Horizontal movement across Base Wall: $1.0	ext{m}$.
  - Horizontal movement across Slanted 60° Walls: $1.0	ext{m}$.
  - Slanted ascending steps: $1.0	ext{m}$ step height check.
  - Drop safety checks: avoid falls $> 3$ blocks unless water is below.
- **Hierarchical Pathfinding (HPA\*)**: Precompute chunk-level gateway graphs to support pathfinding across multiple loaded chunks without frame rate hitching.

### 2. Entity Component System (ECS) Architecture
- Implement a lightweight, cache-friendly data-oriented ECS:
  - `PositionComponent`: $(x, y, z)$, pitch, yaw, velocity.
  - `BoundingPrismComponent`: Height, equilateral radius for collision detection against triangular honeycomb voxels.
  - `HealthComponent`: Current health, max health, invulnerability frames, damage resistance.
  - `AIComponent`: Behavior state machine (Idle, Wander, Target, Attack, Flee).
  - `RenderComponent`: 3D voxel mesh, texture offsets, animation state.

### 3. Mob Manifest
#### Passive Mobs
- **Pig**: 10 HP. Wanders grassland biomes. Attracted to Carrots. Drops Raw Porkchop.
- **Cow**: 10 HP. Drops Raw Beef and Leather. Can be milked with a bucket.
- **Sheep**: 8 HP. Colored wool variants. Regrows wool by eating grass triangular voxels.
- **Chicken**: 4 HP. Floats gently when falling (immune to fall damage). Periodically lays eggs.

#### Hostile Mobs
- **Zombie**: 20 HP. Attacks player on sight. Burns in direct sunlight. Can break wooden doors on hard difficulty.
- **Skeleton**: 20 HP. Ranged combatant. Fires arrows with parabolic trajectory and triangular collision raycasts.
- **Creeper**: 20 HP. Silent approach. Hisses and flashes white when within 3 blocks. Explodes in a 4-block triangular crater.
- **Spider**: 16 HP. Climbs vertical and slanted triangular prism walls. Pounces on targets.

---

## 🌐 Phase 3: Multiplayer & Networking (`v0.3.0`)

### 1. Dedicated Server Architecture
- Headless console application (`PrismCraftServer.exe`) running identical simulation logic without Vulkan or audio dependencies.
- Fixed 20 Hz tick rate ($50	ext{ms}$ per tick) with thread-safe world simulation.

### 2. Protocol & Transport
- Transport via **ENet** (reliable + unreliable UDP channels).
- **Packet Structure**:
  - `C2S_Handshake`: Protocol version validation, player username, authentication token.
  - `C2S_PlayerInput`: Client position, velocity, look angles, held item slot, action intents.
  - `S2C_ChunkData`: Chunk coordinate, compressed voxel data (RLE + zstd).
  - `S2C_BlockChange`: Discrete $(x, y, z, s)$ cell modification.
  - `S2C_EntityState`: Batched entity positions and animations.
  - `C2S_ChatMessage` / `S2C_ChatMessage`: In-game chat broadcast.

### 3. Lag Compensation & Reconciliation
- **Client-Side Prediction**: Local movement responds immediately to input; server validates and sends corrections only on discrepancy.
- **Entity Interpolation**: Remote players and mobs rendered with a 100ms interpolation buffer for jitter-free visual motion under variable ping.

---

## 🌌 Phase 4: Dimensions & Realm Travel (`v0.4.0`)

### 1. The Nether Realm
- **World Generator**: Dual-ceiling cave generation using 3D Perlin noise spanning $Y \in [0, 128]$.
- **Biomes**:
  - *Basalt Deltas*: Columnar basalt prisms, magma cubes, ash particles.
  - *Nether Wastes*: Vast netherrack expanses, glowstone clusters hanging from ceilings, subterranean quartz veins.
  - *Crimson & Warped Forests*: Giant fungal trees, shroomlights, nylium ground cover.
- **Portals**: Triangular obsidian portal frames. Standing inside for 4 seconds triggers dimension teleportation with dynamic coordinate scaling ($1:8$ distance ratio).

### 2. The End Dimension
- Central floating island surrounded by endless void.
- Obsidian monolith pillars topped with End Crystals.
- **Ender Dragon Boss**:
  - 3D segmented spline-animated dragon entity.
  - Circle, dive-bomb, and dragon breath attack patterns.
  - Crystal beam healing mechanic.
  - Defeat triggers Exit Portal generation and Dragon Egg spawn.

---

## ⚡ Phase 5: Triangular Redstone Logic (`v0.5.0`)

### 1. Redstone Signal Propagation Rules
- Signal strength: $15 	o 0$ with 1 strength loss per block.
- In a triangular honeycomb, redstone dust placed on top face of prism $(x, y, z, s)$ connects to:
  - The 3 adjacent horizontal top faces across Base, Left, and Right walls.
  - Upward connections to blocks 1 step higher along slanted walls.
  - Downward connections to blocks 1 step lower.

### 2. Logic Components
- **Redstone Dust**: Visual line/junction rendering adapted for 3-way triangular splitting.
- **Redstone Torch**: Provides constant power 15; inverts when base block is powered.
- **Repeater**: Extends signal back to 15, configurable 1–4 tick delay ($0.1	ext{s} - 0.4	ext{s}$).
- **Comparator**: Compares/subtracts signal strengths; reads container occupancy (chests, furnaces, barrels).
- **Pistons**:
  - Normal Piston: Pushes up to 12 triangular blocks in any of 5 neighbor directions.
  - Sticky Piston: Pushes and pulls connected blocks.

---

## 📦 Phase 6: Extensibility, Modding & Production Polish (`v1.0.0`)

### 1. Modding API
- **Lua Scripting Host**: Embedded LuaJIT or Sol3 bindings exposing:
  - `prismcraft.register_block(def)`
  - `prismcraft.register_item(def)`
  - `prismcraft.register_recipe(recipe)`
  - `prismcraft.register_biome(gen_params)`
  - `prismcraft.on_player_break(fn)`
  - `prismcraft.on_entity_tick(fn)`
- **Dynamic Hot-Reloading**: Modify scripts or JSON definitions live during gameplay without restarting the executable.

### 2. Advanced Graphics
- **Hardware Ray Tracing**: Integration of `VK_KHR_ray_tracing_pipeline` for real-time RT soft shadows, ambient occlusion, and specular reflections on supported GPUs.
- **Cross-Platform Compilation**: Fully modular build configuration supporting Linux x86_64 via Clang, Wayland/X11 windowing, and Vulkan drivers.\n