@echo off
echo Setting up SFML and building the project...

REM Run the setup script first
call setup_sfml.bat

REM Build the project directly with g++
if not exist obj mkdir obj
if not exist obj\engine mkdir obj\engine

REM Compile source files
g++ -Wall -Wextra -O3 -std=c++17 -I. -c src/engine/PerlinNoise.cpp -o obj/engine/PerlinNoise.o
g++ -Wall -Wextra -O3 -std=c++17 -I. -c src/engine/TextureManager.cpp -o obj/engine/TextureManager.o
g++ -Wall -Wextra -O3 -std=c++17 -I. -c src/engine/TerrainGenerator.cpp -o obj/engine/TerrainGenerator.o
g++ -Wall -Wextra -O3 -std=c++17 -I. -c src/main.cpp -o obj/main.o

REM Link the final executable
g++ -o main.exe obj/main.o obj/engine/PerlinNoise.o obj/engine/TextureManager.o obj/engine/TerrainGenerator.o -L. -lsfml-graphics-2 -lsfml-window-2 -lsfml-system-2 -lgdi32 -luser32 -lgdiplus

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Running the application...
    main.exe
) else (
    echo Build failed with error code %ERRORLEVEL%.
    pause
) 