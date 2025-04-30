# PowerShell script for building 2D Terrain Generator

Write-Host "Setting up SFML and building the project..." -ForegroundColor Green

# Run the setup script first
& ./setup_sfml.bat

# Create the obj directories if they don't exist
if (-not (Test-Path "obj")) { New-Item -ItemType Directory -Path "obj" | Out-Null }
if (-not (Test-Path "obj\engine")) { New-Item -ItemType Directory -Path "obj\engine" | Out-Null }

# Compile the source files
Write-Host "Compiling PerlinNoise.cpp..." -ForegroundColor Cyan
& g++ -Wall -Wextra -O3 -std=c++17 -I. -c src/engine/PerlinNoise.cpp -o obj/engine/PerlinNoise.o

Write-Host "Compiling TextureManager.cpp..." -ForegroundColor Cyan
& g++ -Wall -Wextra -O3 -std=c++17 -I. -c src/engine/TextureManager.cpp -o obj/engine/TextureManager.o

Write-Host "Compiling TerrainGenerator.cpp..." -ForegroundColor Cyan
& g++ -Wall -Wextra -O3 -std=c++17 -I. -c src/engine/TerrainGenerator.cpp -o obj/engine/TerrainGenerator.o

Write-Host "Compiling main.cpp..." -ForegroundColor Cyan
& g++ -Wall -Wextra -O3 -std=c++17 -I. -c src/main.cpp -o obj/main.o

# Link the final executable
Write-Host "Linking final executable..." -ForegroundColor Cyan
& g++ -o main.exe obj/main.o obj/engine/PerlinNoise.o obj/engine/TextureManager.o obj/engine/TerrainGenerator.o -L. -lsfml-graphics-2 -lsfml-window-2 -lsfml-system-2 -lgdi32 -luser32 -lgdiplus

# Check if build was successful
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful! Running the application..." -ForegroundColor Green
    & ./main.exe
} else {
    Write-Host "Build failed with error code $LASTEXITCODE." -ForegroundColor Red
    Read-Host "Press Enter to exit"
} 