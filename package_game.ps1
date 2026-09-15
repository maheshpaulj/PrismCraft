<#
.SYNOPSIS
    Packages PrismCraft into a standalone, portable distribution and zip archive.
.DESCRIPTION
    1. Compiles GLSL shaders to SPIR-V.
    2. Builds Release x64 binary via MSBuild.
    3. Bundles PrismCraft.exe, assets, and app-local MSVC C++ Runtime DLLs.
    4. Generates README.txt with controls and specs.
    5. Creates a compressed zip archive ready for sharing/distribution.
#>

param(
    [string]$Version = "0.1.0",
    [string]$OutputDir = "dist",
    [switch]$SkipBuild = $false,
    [switch]$SkipZip = $false
)

$ErrorActionPreference = "Stop"

$projDir = $PSScriptRoot
Set-Location $projDir

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " PrismCraft Standalone Exporter - v$Version" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Compile Shaders & Sync Assets
Write-Host "`n[1/5] Compiling GLSL shaders to SPIR-V..." -ForegroundColor Yellow
$shaderBat = Join-Path $projDir "compile_shaders.bat"
if (Test-Path $shaderBat) {
    & cmd.exe /c "`"$shaderBat`""
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Shader compilation batch exited with code $LASTEXITCODE. Continuing..."
    }
} else {
    Write-Warning "compile_shaders.bat not found. Assuming shaders are already compiled."
}

# 2. Build Release x64 Executable
if (-not $SkipBuild) {
    Write-Host "`n[2/5] Building Release x64 binary via MSBuild..." -ForegroundColor Yellow
    
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuildPath = $null

    if (Test-Path $vswhere) {
        $vsInstall = & $vswhere -latest -property installationPath
        if ($vsInstall) {
            $candidate = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $candidate) { $msbuildPath = $candidate }
        }
    }

    if (-not $msbuildPath) {
        # Fallback to known default paths
        $fallbacks = @(
            "D:\Softwares\Visual Studio\MSBuild\Current\Bin\MSBuild.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        )
        foreach ($fb in $fallbacks) {
            if (Test-Path $fb) {
                $msbuildPath = $fb
                break
            }
        }
    }

    if ($msbuildPath) {
        Write-Host "Using MSBuild: $msbuildPath" -ForegroundColor Gray
        & "$msbuildPath" PrismCraft.vcxproj /p:Configuration=Release /p:Platform=x64 /m
        if ($LASTEXITCODE -ne 0) {
            throw "MSBuild compilation failed with exit code $LASTEXITCODE"
        }
    } else {
        Write-Warning "MSBuild not found. Using existing x64\Release\PrismCraft.exe if available."
    }
} else {
    Write-Host "`n[2/5] Skipping build step (-SkipBuild specified)." -ForegroundColor Gray
}

# Verify Release executable exists
$exePath = Join-Path $projDir "x64\Release\PrismCraft.exe"
if (-not (Test-Path $exePath)) {
    throw "Executable not found at: $exePath. Please build Release x64 first."
}

# 3. Prepare Standalone Directory Structure
Write-Host "`n[3/5] Preparing standalone bundle in '$OutputDir\PrismCraft'..." -ForegroundColor Yellow

$targetDir = Join-Path $projDir (Join-Path $OutputDir "PrismCraft")
if (Test-Path $targetDir) {
    Remove-Item -Path $targetDir -Recurse -Force
}
New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

# Copy PrismCraft.exe
Copy-Item -Path $exePath -Destination (Join-Path $targetDir "PrismCraft.exe") -Force
Write-Host "  -> Copied PrismCraft.exe" -ForegroundColor Green

# 4. Copy Assets (Shaders, Textures, Sounds, Music, Blocks Definition)
$targetAssets = Join-Path $targetDir "assets"
New-Item -ItemType Directory -Path $targetAssets -Force | Out-Null

# Copy blocks.json
$blocksJson = Join-Path $projDir "assets\blocks.json"
if (Test-Path $blocksJson) {
    Copy-Item -Path $blocksJson -Destination (Join-Path $targetAssets "blocks.json") -Force
}

# Copy compiled shaders (*.spv)
$targetShaders = Join-Path $targetAssets "shaders"
New-Item -ItemType Directory -Path $targetShaders -Force | Out-Null
Get-ChildItem -Path (Join-Path $projDir "assets\shaders") -Filter "*.spv" | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination $targetShaders -Force
}
Write-Host "  -> Copied compiled SPIR-V shaders" -ForegroundColor Green

# Copy textures (exclude .psd and backup files)
$targetTextures = Join-Path $targetAssets "textures"
New-Item -ItemType Directory -Path $targetTextures -Force | Out-Null
$srcTextures = Join-Path $projDir "assets\textures"
Get-ChildItem -Path $srcTextures -Recurse | Where-Object { 
    $_.Extension -notin @(".psd", ".bak") -and $_.Name -notmatch "Copy" 
} | ForEach-Object {
    $rel = $_.FullName.Substring($srcTextures.Length).TrimStart("\")
    $dest = Join-Path $targetTextures $rel
    if ($_.PSIsContainer) {
        if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest -Force | Out-Null }
    } else {
        $parent = Split-Path $dest
        if (-not (Test-Path $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
        Copy-Item -Path $_.FullName -Destination $dest -Force
    }
}
Write-Host "  -> Copied game textures & atlas assets" -ForegroundColor Green

# Copy sounds
$targetSounds = Join-Path $targetAssets "sounds"
if (Test-Path (Join-Path $projDir "assets\sounds")) {
    Copy-Item -Path (Join-Path $projDir "assets\sounds") -Destination $targetAssets -Recurse -Force
    Write-Host "  -> Copied sound effects" -ForegroundColor Green
}

# Copy music
$targetMusic = Join-Path $targetAssets "music"
if (Test-Path (Join-Path $projDir "assets\music")) {
    Copy-Item -Path (Join-Path $projDir "assets\music") -Destination $targetAssets -Recurse -Force
    Write-Host "  -> Copied background music" -ForegroundColor Green
}

# 5. Copy App-Local MSVC C++ Runtime DLLs for True Standalone Portability
Write-Host "`n[4/5] Bundling C++ Runtime DLLs for zero-dependency portability..." -ForegroundColor Yellow

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -property installationPath
}
if (-not $vsPath) {
    $vsPath = "D:\Softwares\Visual Studio"
}

$crtSearch = Join-Path $vsPath "VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT"
$crtDirs = Get-Item -Path $crtSearch -ErrorAction SilentlyContinue

if ($crtDirs) {
    $latestCrt = $crtDirs | Select-Object -Last 1
    Get-ChildItem -Path $latestCrt.FullName -Filter "*.dll" | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination $targetDir -Force
    }
    Write-Host "  -> Copied MSVC CRT DLLs from $($latestCrt.Name) (msvcp140.dll, vcruntime140.dll, etc.)" -ForegroundColor Green
} else {
    Write-Warning "Could not find MSVC CRT redistributable DLLs folder. Target PCs may require Microsoft Visual C++ 2015-2022 Redistributable installed."
}

# Write README.txt
$readmeContent = @"
================================================================================
                                 PRISMCRAFT
                      Triangular Prism Voxel Engine
                             Version $Version
================================================================================

ABOUT
-----
PrismCraft is an authentic Minecraft-inspired voxel sandbox game built entirely 
from scratch in C++20 and Vulkan 1.3, powered by a non-cubic equilateral and 
right-triangular prism voxel grid with dynamic lighting, full crafting, 
caves, custom block models, and immersive audio.

HOW TO PLAY
-----------
Simply double-click 'PrismCraft.exe' to launch! No installation required.

CONTROLS
--------
  W, A, S, D       : Walk / Move
  Space            : Jump (Swim up in water)
  Left Shift       : Sneak / Descend
  Left Ctrl        : Sprint
  Left Mouse Click : Mine / Attack
  Right Mouse Click: Place block / Interact (Doors, Trapdoors, Crafting Table, Bed)
  1 - 9 Keys       : Select hotbar slot
  Mouse Scroll     : Cycle hotbar items
  E                : Open Inventory / 2x2 Crafting Grid
  Escape           : Pause Game / Menu / Close Dialog
  F3               : Toggle Debug Overlay (Coordinates, FPS, Biome, Light)
  F11              : Toggle Fullscreen Mode

SYSTEM REQUIREMENTS
-------------------
  Operating System: Windows 10 or Windows 11 (64-bit)
  Graphics Card   : Vulkan 1.3 compatible GPU with updated drivers
                    - NVIDIA GeForce GTX 600 series or newer
                    - AMD Radeon HD 7000 series or newer
                    - Intel HD Graphics 500 / UHD / Iris Xe / Arc
  RAM             : 4 GB minimum (8 GB recommended)
  Storage         : ~100 MB free disk space

SAVED WORLDS & SETTINGS
-----------------------
World saves and game configuration options ('options.txt') are automatically 
stored in the game folder. You can safely copy or backup this folder at any time.

================================================================================
"@
Set-Content -Path (Join-Path $targetDir "README.txt") -Value $readmeContent -Encoding UTF8
Write-Host "  -> Generated README.txt" -ForegroundColor Green

# 6. Create Compressed ZIP Archive
if (-not $SkipZip) {
    Write-Host "`n[5/5] Creating standalone ZIP distribution archive..." -ForegroundColor Yellow
    $zipName = "PrismCraft_v$($Version)_Windows_x64.zip"
    $zipPath = Join-Path (Join-Path $projDir $OutputDir) $zipName
    
    if (Test-Path $zipPath) {
        Remove-Item -Path $zipPath -Force
    }

    Compress-Archive -Path "$targetDir\*" -DestinationPath $zipPath -CompressionLevel Optimal
    
    $zipSize = (Get-Item $zipPath).Length / 1MB
    Write-Host "  -> Created archive: $zipName ($([Math]::Round($zipSize, 2)) MB)" -ForegroundColor Green
}

$dirSize = (Get-ChildItem -Path $targetDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB

Write-Host "`n========================================================" -ForegroundColor Cyan
Write-Host " SUCCESS: Standalone Export Complete!" -ForegroundColor Green
Write-Host " Standalone Folder : $targetDir ($([Math]::Round($dirSize, 2)) MB)" -ForegroundColor White
if (-not $SkipZip) {
    Write-Host " Portable ZIP File : $zipPath" -ForegroundColor White
}
Write-Host "========================================================" -ForegroundColor Cyan
