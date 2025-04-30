# 2D Terrain Generator

A simple 2D terrain generator using SFML and Perlin noise to create hills and caves.

## Features

- Procedural hill generation using Perlin noise
- Cave generation using Perlin noise
- Efficient rendering (only visible tiles)
- Simple camera controls

## Building the Project

### Prerequisites

- MinGW with g++ compiler
- SFML DLLs (already included in the project directory)

### Build Instructions

The project uses the SFML DLLs that are already in the project root directory:
- sfml-graphics-2.dll
- sfml-window-2.dll
- sfml-system-2.dll

To build the project:

```
mingw32-make
```

Alternatively, you can use the build and run script:

```
.\build_and_run.bat
```

## Running the Project

```
.\main.exe
```

## Controls

- WASD/Arrow Keys: Move camera
- R: Regenerate terrain with a new seed
- ESC: Exit

## Implementation Details

- Uses Perlin noise for both height map and cave generation
- Optimized to only draw visible tiles
- Scales textures to match the desired tile size
- Efficient terrain rendering within the visible view area

## Textures

This project uses textures from Kenney's Voxel Pack. 