#ifndef PIECES_PLACEMENT_H
#define PIECES_PLACEMENT_H

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <vector>
#include <memory>

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

// Class to manage piece textures
class PieceTextureManager {
private:
    static PieceTextureManager* instance;
    std::map<std::string, sf::Texture> textures;
    float currentScale;
    
    // Private constructor for singleton
    PieceTextureManager() : currentScale(1.0f) {}
    
public:
    static PieceTextureManager& getInstance();
    bool loadTextures(float scaleFactor = 1.0f);
    const sf::Texture* getTexture(const std::string& key) const;
    float getScale() const { return currentScale; }
};

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

// Backwards compatibility function
bool loadPieceTextures(float scaleFactor = 1.0f);

#endif // PIECES_PLACEMENT_H 