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
- **Cross-Platform Compilation**: Fully modular build configuration supporting Linux x86_64 via Clang, Wayland/X11 windowing, and Vulkan drivers.

---

## 📌 Comprehensive Backlog & Feature Specifications

### 🛡️ 1. Armor System & Boating Mechanics
- **Complete Armor Tiers**:
  - Tiers: Leather, Iron, Gold, Diamond, Netherite.
  - Slots: Helmet, Chestplate, Leggings, Boots.
  - Armor Points: 0 to 20 defense points represented on the HUD with triangular shield icons.
  - Damage Reduction Formula: Authentic Minecraft reduction curve: $\text{damageTaken} = \text{damage} \times (1 - \min(20, \max(\text{armor} / 5, \text{armor} - \frac{\text{damage}}{2})) / 25)$.
  - Durability: Degrades per hit taken; breaking sound effect on depletion.
- **3D Wooden Boats**:
  - Boat Variants: Oak, Spruce, Birch, Jungle, Acacia, Dark Oak.
  - Triangular Fluid Physics: Buoyant floating body interacting with cellular water currents.
  - Controls: Dual-paddle rowing controls (A/D for left/right paddles, W for both).
  - Passenger Seating: Supports player and 1 passive mob.

### 🏔️ 2. Advanced 3D Terrain Generation & Caverns
- **3D Noise Overhangs**: Replace purely 2D heightmaps with multi-octave 3D Simplex noise densities ($y \in [0, 128]$) allowing arches, cliffs, and hollow overhangs.
- **3D Cave Systems**:
  - *Cheese Caves*: Large open subterranean chambers formed by low 3D noise densities.
  - *Spaghetti Caves*: Narrow, sinuous tunnels weaving through deep underground layers.
  - *Cave Biomes*: Dripstone caves with stalactites/stalagmites, Lush caves with moss, glow berries, and hanging vines.
- **Continuous River Systems**: Voronoi-guided watercourses carving smooth canyon channels across biomes towards ocean sea level.

### ⚡ 3. Core Engine Performance Improvements
- **Multithreaded Greedy Mesher**:
  - Parallel chunk meshing across thread pool workers with lock-free task queues.
  - Equilateral triangular 2D face merging along Base, Left, and Right faces to dramatically reduce vertex counts.
- **Indirect GPU Drawing**:
  - Transition from individual draw calls to `vkCmdDrawIndexedIndirect` with GPU-side frustum culling.
  - Multi-draw batching per material pipeline (Opaque, Water, Cutout).
- **GPU Occlusion Culling**: Compute shader generating a 2-pass Hierarchical Z-Buffer (Hi-Z) to discard occluded underground chunks before vertex shading.

### 🚀 4. Native AMD FidelityFX Super Resolution (FSR 2/3) SDK
- **Replace Prototype Spatial Scaler**:
  - Replace the current rudimentary spatial upscaler with the official **AMD FidelityFX FSR 2.2 / FSR 3** C++ SDK.
- **Engine G-Buffer Requirements**:
  - 16-bit 2D Motion Vector buffer ($\text{RG16\_SFLOAT}$) tracking camera and entity screen-space velocities.
  - High-precision depth buffer ($\text{D32\_SFLOAT}$) reprojection.
  - Sub-pixel camera projection matrix jittering using the Halton $(2, 3)$ sequence.
  - Reactive Mask buffer identifying dynamic water, flame particles, and transparent cutouts.
- **FSR Presets**: Native support for Ultra Quality, Quality, Balanced, and Ultra Performance with auto-exposure and RCAS sharpening.

### 🔺 5. Authentic Triangular UI & HUD Theme
- **Geometric Triangular HUD**:
  - Health: 10 equilateral triangular hearts (with half-heart fills and poison/wither tints).
  - Hunger: 10 triangular icons reflecting nutritional saturation.
  - Armor Bar: 10 triangular shield icons above health.
  - Breath Meter: 10 triangular bubble icons when submerged.
- **Triangular Inventory & Menus**:
  - Hotbar: Hexagonal / interlocking triangular cell frames reflecting the equilateral prism lattice.
  - Pause & Settings Menus: Isometric triangular panel styling with sharp 60° beveled border accents.

### 🎨 6. Faithful Flowing Triangular Textures
- **Equilateral Texture Alignment**:
  - Complete overhaul of block face UV coordinates to align with 60° equilateral triangle symmetry.
  - Eliminates stretching and rectangular distortion across triangular prism top and bottom faces.
  - Seamless flowing textures across adjacent 60° slanted walls and horizontal floors.

### 🏛️ 7. Procedural Structures: Villages & Desert Temples
- **Procedural Triangular Villages**:
  - Multi-building settlement generation in Plains, Desert, and Savanna biomes.
  - Village Centers with town wells, cobblestone/path triangular road grids, lamp posts.
  - Diverse Architecture: Villager houses, libraries, butcher shops, blacksmiths with lava forges and loot chests.
  - Farmland plots with wheat, carrots, potatoes, and irrigation channels.
- **Desert Temples (Pyramids)**:
  - Sandstone and orange/blue terracotta stepped pyramids.
  - Symmetrical entrance towers and central altar chamber.
  - Hidden basement crypt with pressure-plate TNT booby traps and 4 high-value treasure chests.
- **Underground Dungeons**:
  - Cobblestone and mossy cobblestone chambers with monster spawners and double loot chests.

### 👾 8. Complete Mob AI & Entity Framework
- **Triangular Honeycomb A\* Pathfinding**:
  - Navigation mesh traversal across the 5-neighbor triangular honeycomb graph.
  - Slanted climb evaluations and jump-step calculations for 60° terrain slopes.
- **Mob Ecosystem**:
  - Passive: Cows (milk, beef, leather), Pigs (saddle ride, pork), Sheep (16 wool colors), Chickens (eggs, feathers).
  - Hostile: Zombies (daylight burn, door siege), Skeletons (trajectory bow aiming), Creepers (proximity hiss and triangular crater explosions), Spiders (wall climbing).

### 🌌 9. Other Dimensions: The Nether & The End
- **The Nether**:
  - Triangular Obsidian Portals (minimum 4×5 frame activated by Flint and Steel).
  - Nether Fortresses: Elevated bridge networks, Nether Wart rooms, Blaze spawners.
  - Biomes: Nether Wastes, Crimson Forest, Warped Forest, Soul Sand Valley, Basalt Deltas.
- **The End**:
  - Stronghold End Portal frames requiring Eyes of Ender.
  - Obsidian monolith towers with healing End Crystals.
  - Fully animated Ender Dragon boss encounter, Dragon Breath particle clouds, and exit portal fountain.\n