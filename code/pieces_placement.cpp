#include "pieces_placement.h"
#include <iostream>
#include <filesystem>

// Global texture map to store all piece textures
std::map<std::string, sf::Texture> pieceTextures;
float currentPieceScale = 1.0f; // Store current scale factor

// Mapping of FEN characters to piece types
const std::map<char, PieceType> FEN_TO_PIECE_TYPE = {
    {'p', PieceType::PAWN},   {'P', PieceType::PAWN},
    {'r', PieceType::ROOK},   {'R', PieceType::ROOK},
    {'n', PieceType::KNIGHT}, {'N', PieceType::KNIGHT},
    {'b', PieceType::BISHOP}, {'B', PieceType::BISHOP},
    {'q', PieceType::QUEEN},  {'Q', PieceType::QUEEN},
    {'k', PieceType::KING},   {'K', PieceType::KING}
};

// Get list of piece filenames
std::vector<std::string> getPieceFilenames() {
    return {
        "white-pawn", "white-rook", "white-knight", "white-bishop", "white-queen", "white-king",
        "black-pawn", "black-rook", "black-knight", "black-bishop", "black-queen", "black-king"
    };
}

// Function to load a single texture with error handling
bool loadTextureWithErrorHandling(const std::string& filename, sf::Texture& texture) {
    // Verify file exists
    std::filesystem::path filePath(filename);
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "Texture file not found: " << filename << std::endl;
        return false;
    }

    if (!texture.loadFromFile(filename)) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        return false;
    }
    return true;
}

// Function to load all piece textures (using hyphen format)
bool loadPieceTextures(float scaleFactor) {
    bool allLoaded = true;
    currentPieceScale = scaleFactor; // Store the scale factor for future use
    
    // Check if pieces directory exists
    std::filesystem::path piecesDir("./pieces");
    if (!std::filesystem::exists(piecesDir)) {
        std::cerr << "Pieces directory not found. Creating it..." << std::endl;
        try {
            std::filesystem::create_directory(piecesDir);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create pieces directory: " << e.what() << std::endl;
        }
        return false;
    }
    
    // Get all piece filenames
    const auto pieces = getPieceFilenames();
    
    // Load each texture
    for (const auto& piece : pieces) {
        std::string filename = "./pieces/" + piece + ".png";
        sf::Texture texture;
        if (loadTextureWithErrorHandling(filename, texture)) {
            texture.setSmooth(true); // Enable smooth scaling
            pieceTextures[piece] = texture;
        } else {
            allLoaded = false;
        }
    }
    
    return allLoaded;
}

// Get piece color from FEN character
PieceColor getColorFromFEN(char fenChar) {
    return std::isupper(fenChar) ? PieceColor::WHITE : PieceColor::BLACK;
}

// Get piece texture key from FEN character
std::string getTextureKeyFromFEN(char fenChar) {
    std::string color = std::isupper(fenChar) ? "white" : "black";
    
    // Map of piece characters to piece names
    const std::map<char, std::string> pieceMap = {
        {'p', "pawn"}, {'r', "rook"}, {'n', "knight"}, 
        {'b', "bishop"}, {'q', "queen"}, {'k', "king"}
    };
    
    char lowerChar = std::tolower(fenChar);
    if (pieceMap.find(lowerChar) != pieceMap.end()) {
        return color + "-" + pieceMap.at(lowerChar);
    }
    
    return ""; // Invalid character
}

// Function to set up pieces according to FEN notation
void setupPositionFromFEN(std::map<std::pair<int, int>, ChessPiece>& pieces, const std::string& fen) {
    pieces.clear();
    const float SQUARE_SIZE = 100.0f;
    
    // Parse board position (first part of FEN)
    std::string boardFEN = fen.substr(0, fen.find(' '));
    
    int row = 0;
    int col = 0;
    
    for (char c : boardFEN) {
        if (c == '/') {
            // Move to next row
            row++;
            col = 0;
        } else if (std::isdigit(c)) {
            // Skip empty squares
            col += c - '0';
        } else if (std::isalpha(c)) {
            // Place a piece
            std::string textureKey = getTextureKeyFromFEN(c);
            
            if (!textureKey.empty() && pieceTextures.find(textureKey) != pieceTextures.end()) {
                ChessPiece piece{
                    FEN_TO_PIECE_TYPE.at(c),
                    getColorFromFEN(c),
                    sf::Sprite(pieceTextures[textureKey])
                };
                
                // Calculate center positions to place pieces
                float textureWidth = piece.sprite.getTexture()->getSize().x;
                float textureHeight = piece.sprite.getTexture()->getSize().y;
                
                // Apply the scaling factor (10% bigger)
                float scaleX = (SQUARE_SIZE / textureWidth) * currentPieceScale;
                float scaleY = (SQUARE_SIZE / textureHeight) * currentPieceScale;
                piece.sprite.setScale(scaleX, scaleY);
                
                // Center the piece in its square
                float offsetX = (SQUARE_SIZE - (textureWidth * scaleX)) / 2;
                float offsetY = (SQUARE_SIZE - (textureHeight * scaleY)) / 2;
                
                piece.sprite.setPosition(
                    col * SQUARE_SIZE + offsetX,
                    row * SQUARE_SIZE + offsetY
                );
                
                pieces[{col, row}] = piece;
            }
            
            col++;
        }
    }
}

// Function to draw pieces on the board
void drawPieces(sf::RenderWindow& window, const std::map<std::pair<int, int>, ChessPiece>& pieces) {
    for (const auto& [position, piece] : pieces) {
        window.draw(piece.sprite);
    }
} 