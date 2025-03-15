# Chess Board Application

A simple chess board application built with SFML that displays chess pieces in the standard starting position.

## Build Instructions

1. Make sure SFML is properly installed and configured
2. Use MinGW to build the project:
   ```
   mingw32-make -f MakeFile
   ```
3. Run the application:
   ```
   ./chess_board
   ```

## Git Checkpoint Feature

The project includes a Git checkpoint feature to save your work. To create a checkpoint:

```
mingw32-make -f MakeFile git-checkpoint
```

This will initialize a Git repository (if not already done) and create a commit with all the current files.

## Special Features

- Chess pieces are displayed 10% larger for better visibility
- Smooth anti-aliased rendering for better visual quality
- Error handling for missing texture files
- Proper centering of pieces on the board

## File Structure

- `code/` - Contains all source code files
- `pieces/` - Should contain all chess piece images (format: white-pawn.png, black-knight.png, etc.)
- `MakeFile` - Build configuration
- `chess_board` - The compiled executable

## Troubleshooting

If you encounter issues with missing piece images, ensure the `pieces` directory exists with properly named PNG images for each chess piece. 