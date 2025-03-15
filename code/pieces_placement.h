#ifndef PIECES_PLACEMENT_H
#define PIECES_PLACEMENT_H

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <vector>
#include <filesystem>

// Enumeration for piece types
enum class PieceType {
    PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING
};

// Enumeration for piece colors
enum class PieceColor {
    WHITE, BLACK
};

// Struct to define a chess piece
struct ChessPiece {
    PieceType type;
    PieceColor color;
    sf::Sprite sprite;
};

// Map to convert FEN character to piece type
extern const std::map<char, PieceType> FEN_TO_PIECE_TYPE;

// Function to load all piece textures (now with scaling factor)
bool loadPieceTextures(float scaleFactor = 1.0f);

// Function to set up piece positions from FEN notation
// Standard starting position FEN: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
void setupPositionFromFEN(std::map<std::pair<int, int>, ChessPiece>& pieces, 
                          const std::string& fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

// Function to draw pieces on the board
void drawPieces(sf::RenderWindow& window, const std::map<std::pair<int, int>, ChessPiece>& pieces);

// External texture map to store all piece textures
extern std::map<std::string, sf::Texture> pieceTextures;

// Get list of piece filenames
std::vector<std::string> getPieceFilenames();

// Helper functions for FEN notation
PieceColor getColorFromFEN(char fenChar);
std::string getTextureKeyFromFEN(char fenChar);

#endif // PIECES_PLACEMENT_H 