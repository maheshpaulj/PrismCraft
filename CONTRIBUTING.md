# 🤝 Contributing to PrismCraft

Thank you for your interest in contributing to **PrismCraft**! As an engine built from scratch on novel geometric foundations, we maintain rigorous engineering standards for code clarity, performance, and memory safety.

---

## 🏛️ Code Architecture & Guidelines

### 1. Language Standards
- **C++ Standard**: Strictly **C++20** (`/std:c++20` on MSVC).
- **Modern Features Encouraged**:
  - `std::span`, `std::string_view` for non-owning views.
  - Structured bindings `auto [x, y] = ...`
  - `[[nodiscard]]` on all pure queries and getters.
  - `constexpr` and `consteval` where computationally feasible.
- **Naming Conventions**:
  - Types / Classes / Structs: `PascalCase` (`ChunkMesher`, `VulkanContext`)
  - Member Variables: `m_camelCase` (`m_swapchain`, `m_commandPool`)
  - Static Variables: `s_camelCase` (`s_instance`)
  - Constants: `UPPER_SNAKE_CASE` (`CHUNK_SIZE_X`, `TRI_HEIGHT`)
  - Functions / Methods: `camelCase` (`buildAtlas()`, `getTileUV()`)
  - Namespaces: `prismcraft` (all lowercase, no nested clutter)

### 2. Vulkan Resource Management & Safety
- **Zero Raw Leaks (RAII)**: Every Vulkan resource (`VkImage`, `VkBuffer`, `VkPipeline`, `VkDescriptorSet`) must be owned by an RAII container.
- **Vulkan Memory Allocator (VMA)**: Use VMA for all device allocations (`vmaCreateBuffer`, `vmaCreateImage`). Manual `vkAllocateMemory` is prohibited.
- **Dynamic Rendering**: Do **not** create legacy `VkRenderPass` or `VkFramebuffer` objects. Use `vkCmdBeginRendering` / `vkCmdEndRendering` per Vulkan 1.3 standards.
- **Error Handling**: Wrap all Vulkan API calls with `VK_CHECK(res, "error message")`.

### 3. Triangular Prism Grid Conventions
When working with world geometry, remember the core principles of the equilateral honeycomb:
- Position is defined as `(x, y, z, s)` where `s ∈ {0, 1}`.
- Prism `s = 0` points North (+Z); prism `s = 1` points South (-Z).
- Each cell has **5 neighbors**, indexed `0..4` (Top, Bottom, Base wall, Left wall, Right wall).
- Always use `getPrismVerticesXZ(x, z, s, v)` from `Coordinates.hpp` for geometric calculations.
- Always use the **Unified Normal Triangle** convention for top and bottom face UV coordinates to prevent inverted textures.

---

## 🛠️ Development Workflow

### 1. Branching Model
- `master`: Stable, deployable releases.
- `develop` or feature branches: `feature/<feature-name>`, `fix/<bug-description>`.

### 2. Making Changes
1. Fork or branch from `master`.
2. Make targeted, focused modifications.
3. If adding or modifying shaders in `assets/shaders/`, compile them with `compile_shaders.bat` and verify that `glslc` reports 0 errors or warnings.
4. If adding new block types, add them to `assets/blocks.json` and ensure textures exist in `assets/textures/textures/block/`.
5. Build the Release configuration via MSBuild:
   ```powershell
   & "D:\Softwares\Visual Studio\MSBuild\Current\Bin\MSBuild.exe" PrismCraft.vcxproj /p:Configuration=Release /p:Platform=x64 /m
   ```
6. Run the game and verify with the F3 debug HUD that frame rate and memory remain stable.

### 3. Packaging & Verifying
Before opening a PR, verify the standalone packaging script:
```powershell
.\package_game.ps1 -Version "0.1.0" -SkipBuild
```
Ensure the self-contained folder runs cleanly without external dependencies.\n