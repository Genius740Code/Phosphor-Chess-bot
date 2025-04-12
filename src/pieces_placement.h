#ifndef PIECES_PLACEMENT_H
#define PIECES_PLACEMENT_H

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <vector>

// Piece type and color enumerations
enum class PieceType { PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING };
enum class PieceColor { WHITE, BLACK };

// Chess piece structure
struct ChessPiece {
    PieceType type;
    PieceColor color;
    sf::Sprite sprite;
};

// Singleton texture manager for chess pieces
class PieceTextureManager {
private:
    static PieceTextureManager* instance;
    std::map<std::string, sf::Texture> textures;
    float currentScale;
    
    PieceTextureManager() : currentScale(1.0f) {}
    
public:
    static PieceTextureManager& getInstance();
    bool loadTextures(float scaleFactor = 1.0f);
    const sf::Texture* getTexture(const std::string& key) const;
    float getScale() const { return currentScale; }
};

// Position setup and drawing
void setupPositionFromFEN(std::map<std::pair<int, int>, ChessPiece>& pieces, 
                          const std::string& fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
void drawPieces(sf::RenderWindow& window, const std::map<std::pair<int, int>, ChessPiece>& pieces);

// Helper functions for FEN notation
PieceColor getColorFromFEN(char fenChar);
std::string getTextureKeyFromFEN(char fenChar);

// Backwards compatibility
bool loadPieceTextures(float scaleFactor = 1.0f);

#endif // PIECES_PLACEMENT_H 