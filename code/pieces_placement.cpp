#include "pieces_placement.h"
#include <iostream>
#include <filesystem>

// Initialize static instance pointer
PieceTextureManager* PieceTextureManager::instance = nullptr;

// Get singleton instance
PieceTextureManager& PieceTextureManager::getInstance() {
    if (!instance) {
        instance = new PieceTextureManager();
    }
    return *instance;
}

// Get a texture from the manager
const sf::Texture* PieceTextureManager::getTexture(const std::string& key) const {
    auto it = textures.find(key);
    if (it != textures.end()) {
        return &(it->second);
    }
    return nullptr;
}

// Load all piece textures
bool PieceTextureManager::loadTextures(float scaleFactor) {
    currentScale = scaleFactor;
    textures.clear();
    
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
    
    // Piece filenames 
    const std::vector<std::string> pieceNames = {
        "white-pawn", "white-rook", "white-knight", "white-bishop", "white-queen", "white-king",
        "black-pawn", "black-rook", "black-knight", "black-bishop", "black-queen", "black-king"
    };
    
    bool allLoaded = true;
    
    // Load each texture
    for (const auto& piece : pieceNames) {
        std::string filename = "./pieces/" + piece + ".png";
        
        // Verify file exists
        std::filesystem::path filePath(filename);
        if (!std::filesystem::exists(filePath)) {
            std::cerr << "Texture file not found: " << filename << std::endl;
            allLoaded = false;
            continue;
        }
        
        sf::Texture texture;
        if (!texture.loadFromFile(filename)) {
            std::cerr << "Failed to load texture: " << filename << std::endl;
            allLoaded = false;
            continue;
        }
        
        texture.setSmooth(true); // Enable smooth scaling
        textures[piece] = std::move(texture);
    }
    
    return allLoaded;
}

// Mapping of FEN characters to piece types
const std::map<char, PieceType> FEN_TO_PIECE_TYPE = {
    {'p', PieceType::PAWN},   {'P', PieceType::PAWN},
    {'r', PieceType::ROOK},   {'R', PieceType::ROOK},
    {'n', PieceType::KNIGHT}, {'N', PieceType::KNIGHT},
    {'b', PieceType::BISHOP}, {'B', PieceType::BISHOP},
    {'q', PieceType::QUEEN},  {'Q', PieceType::QUEEN},
    {'k', PieceType::KING},   {'K', PieceType::KING}
};

// Backwards compatibility function
bool loadPieceTextures(float scaleFactor) {
    return PieceTextureManager::getInstance().loadTextures(scaleFactor);
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
    auto& textureManager = PieceTextureManager::getInstance();
    
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
            const sf::Texture* texture = textureManager.getTexture(textureKey);
            
            if (!textureKey.empty() && texture) {
                ChessPiece piece{
                    FEN_TO_PIECE_TYPE.at(c),
                    getColorFromFEN(c),
                    sf::Sprite(*texture)
                };
                
                // Calculate center positions to place pieces
                float textureWidth = texture->getSize().x;
                float textureHeight = texture->getSize().y;
                
                // Apply the scaling factor
                float scaleFactor = textureManager.getScale();
                float scaleX = (SQUARE_SIZE / textureWidth) * scaleFactor;
                float scaleY = (SQUARE_SIZE / textureHeight) * scaleFactor;
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