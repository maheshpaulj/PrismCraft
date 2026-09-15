@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo PrismCraft Shader Compiler
echo ========================================================

set "GLSLC_EXE=D:\Softwares\Vulkan SDK\Bin\glslc.exe"
if not exist "!GLSLC_EXE!" (
    if defined VULKAN_SDK (
        set "GLSLC_EXE=%VULKAN_SDK%\Bin\glslc.exe"
    ) else (
        set "GLSLC_EXE=glslc.exe"
    )
)

echo Using glslc: !GLSLC_EXE!
echo.

set "PROJ_DIR=%~dp0"
set "SRC_SHADERS=%PROJ_DIR%assets\shaders"
set "REL_SHADERS=%PROJ_DIR%x64\Release\assets\shaders"
set "DBG_SHADERS=%PROJ_DIR%x64\Debug\assets\shaders"

if not exist "!REL_SHADERS!" mkdir "!REL_SHADERS!"
if not exist "!DBG_SHADERS!" mkdir "!DBG_SHADERS!"

for %%F in ("!SRC_SHADERS!\*.vert" "!SRC_SHADERS!\*.frag" "!SRC_SHADERS!\*.comp") do (
    echo Compiling %%~nxF -^> %%~nxF.spv...
    "!GLSLC_EXE!" "%%F" -o "!SRC_SHADERS!\%%~nxF.spv"
    if errorlevel 1 (
        echo [ERROR] Failed to compile %%~nxF!
    ) else (
        copy /Y "!SRC_SHADERS!\%%~nxF.spv" "!REL_SHADERS!\%%~nxF.spv" >nul 2>nul
        copy /Y "!SRC_SHADERS!\%%~nxF.spv" "!DBG_SHADERS!\%%~nxF.spv" >nul 2>nul
    )
)

echo Syncing textures, sounds, music, and block definitions...
for %%D in ("%PROJ_DIR%x64\Release" "%PROJ_DIR%x64\Debug") do (
    if not exist "%%~D\assets\textures" mkdir "%%~D\assets\textures"
    if not exist "%%~D\assets\sounds" mkdir "%%~D\assets\sounds"
    if not exist "%%~D\assets\music" mkdir "%%~D\assets\music"
    copy /Y "%PROJ_DIR%assets\blocks.json" "%%~D\assets\blocks.json" >nul 2>nul
    xcopy /E /I /Y "%PROJ_DIR%assets\textures" "%%~D\assets\textures" >nul 2>nul
    copy /Y "%PROJ_DIR%assets\sounds\*" "%%~D\assets\sounds\" >nul 2>nul
    copy /Y "%PROJ_DIR%assets\music\*" "%%~D\assets\music\" >nul 2>nul
)

echo.
echo ========================================================
echo Shader compilation complete! Synced to Debug and Release.
echo ========================================================
