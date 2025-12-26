@echo off
setlocal enabledelayedexpansion

:: Since the script is in the same folder as the shaders
set "OUTPUT_DIR=spirV"

echo ------------------------------------------
echo  Imaginatrix Engine: Shader Compilation
echo ------------------------------------------

:: Create SPIR-V output directory if it doesn't exist
if not exist "%OUTPUT_DIR%" (
    echo Creating directory: %OUTPUT_DIR%
    mkdir "%OUTPUT_DIR%"
)

:: Compile Vertex Shaders (.vert)
for %%f in (*.vert) do (
    echo Compiling Vertex: %%f
    glslangValidator -V --target-env vulkan1.3 "%%f" -o "%OUTPUT_DIR%/%%f.spv"
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to compile %%f
        pause
        exit /b !errorlevel!
    )
)

:: Compile Fragment Shaders (.frag)
for %%f in (*.frag) do (
    echo Compiling Fragment: %%f
    glslangValidator -V --target-env vulkan1.3 "%%f" -o "%OUTPUT_DIR%/%%f.spv"
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to compile %%f
        pause
        exit /b !errorlevel!
    )
)

:: Compile Compute Shaders (.comp)
for %%f in (*.comp) do (
    echo Compiling Compute: %%f
    glslangValidator -V --target-env vulkan1.3 "%%f" -o "%OUTPUT_DIR%/%%f.spv"
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to compile %%f
        pause
        exit /b !errorlevel!
    )
)

echo.
echo [SUCCESS] All shaders compiled to %OUTPUT_DIR%
echo ------------------------------------------
pause