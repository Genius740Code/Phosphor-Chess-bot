# Chess Game with SFML

A feature-rich chess game built with C++ and SFML graphics library. This game provides a complete chess experience with all standard chess rules implemented, including special moves like castling, en passant, and pawn promotion.

![Chess Game Screenshot](screenshot.png)

## Features

- Complete chess rules implementation
- Visual highlighting of legal moves
- Check and checkmate detection
- All draw conditions (stalemate, insufficient material, 50-move rule, threefold repetition)
- Pawn promotion interface
- Castling (kingside and queenside)
- En passant captures
- Move validation
- Game state display
- Performance optimizations

## Installation

### Prerequisites

- MinGW-w64 with g++ compiler (for Windows)
- SFML 2.5.1 or newer
- Git (optional, for version control)

### Windows Installation

1. **Install MinGW-w64**:
   - Download and install MinGW-w64 from [MinGW-w64 website](https://www.mingw-w64.org/downloads/)
   - Add MinGW bin directory to your PATH environment variable

2. **Install SFML**:
   - Download SFML for MinGW from the [SFML website](https://www.sfml-dev.org/download.php)
   - Extract the files to a location on your computer
   - Copy the SFML DLLs to your project directory or ensure they're in your PATH

3. **Clone the repository**:
   ```
   git clone https://github.com/yourusername/chess-game.git
   cd chess-game
   ```

4. **Build the project**:
   ```
   mingw32-make -f MakeFile
   ```

5. **Run the game**:
   ```
   ./main
   ```

### Linux Installation

1. **Install dependencies**:
   ```
   sudo apt-get update
   sudo apt-get install libsfml-dev g++ make
   ```

2. **Clone the repository**:
   ```
   git clone https://github.com/yourusername/chess-game.git
   cd chess-game
   ```

3. **Build the project**:
   ```
   make -f MakeFile
   ```

4. **Run the game**:
   ```
   ./main
   ```

## Game Controls

- **Left Mouse Button**: Select and move pieces
- Click on a piece to select it, then click on a highlighted square to make a move
- When a pawn reaches the opposite end of the board, click on your preferred promotion piece
- The game state indicator shows whose turn it is and if a player is in check
- Use the "New Game" button to start a fresh game

## Project Structure

- `src/`: Source code files
  - `main.cpp`: Entry point of the application
  - `gui.cpp/.h`: UI handling and main window management
  - `game_logic.cpp/.h`: Chess rules and game state management
  - `pieces_placement.cpp/.h`: Piece handling and board setup
  - `pieces_movment.cpp/.h`: Move validation and execution

## Creating Chess Piece Images

For the game to display correctly, you'll need chess piece images. Create a `pieces` folder in the same directory as the executable and add the following PNG image files:

- white-pawn.png
- white-rook.png
- white-knight.png
- white-bishop.png
- white-queen.png
- white-king.png
- black-pawn.png
- black-rook.png
- black-knight.png
- black-bishop.png
- black-queen.png
- black-king.png

You can use any chess piece images, but they should be properly sized and have transparent backgrounds for best results.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgements

- [SFML](https://www.sfml-dev.org/) for providing the graphics framework
- Chess piece designs from [Chess.com](https://www.chess.com/) (if you use their designs)

## Git Checkpoint Feature

The project includes a Git checkpoint feature to save your work. To create a checkpoint:

```