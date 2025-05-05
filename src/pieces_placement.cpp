#include "pieces_placement.h"
#include <iostream>
#include <filesystem>
#include <unordered_map>

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
    return it != textures.end() ? &(it->second) : nullptr;
}

// Load all piece textures
bool PieceTextureManager::loadTextures(float scaleFactor) {
    currentScale = scaleFactor;
    textures.clear();
    
    // Check if pieces directory exists
    std::filesystem::path piecesDir("./pieces");
    // Try root directory first, then check in build/exe/pieces
    if (!std::filesystem::exists(piecesDir)) {
        // Try alternative paths
        std::vector<std::filesystem::path> altPaths = {
            "../pieces",
            "../../pieces",
            "build/exe/pieces"
        };
        
        bool foundDir = false;
        for (const auto& path : altPaths) {
            if (std::filesystem::exists(path)) {
                piecesDir = path;
                std::cout << "Found pieces directory at: " << path.string() << std::endl;
                foundDir = true;
                break;
            }
        }
        
        if (!foundDir) {
            std::cerr << "Pieces directory not found. Creating it..." << std::endl;
            try {
                std::filesystem::create_directory(piecesDir);
                
                std::cerr << "Pieces directory created at: " << std::filesystem::absolute(piecesDir) << std::endl;
                std::cerr << "Please copy chess piece PNG images to this directory." << std::endl;
                std::cerr << "Required files: white-pawn.png, white-rook.png, etc." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to create pieces directory: " << e.what() << std::endl;
            }
            return false;
        }
    }
    
    // Piece filenames array for faster iteration
    static const std::string pieceNames[] = {
        "white-pawn", "white-rook", "white-knight", "white-bishop", "white-queen", "white-king",
        "black-pawn", "black-rook", "black-knight", "black-bishop", "black-queen", "black-king"
    };
    
    bool allLoaded = true;
    bool anyLoaded = false;
    
    // Create fallback textures for missing pieces
    sf::Texture fallbackWhite;
    sf::Texture fallbackBlack;
    
    // Create simple fallback textures (10x10 white/black squares)
    sf::Image whiteImg;
    sf::Image blackImg;
    whiteImg.create(20, 20, sf::Color::White);
    blackImg.create(20, 20, sf::Color::Black);
    
    // Add a simple border to fallback textures to make them visible on both square colors
    for (unsigned int x = 0; x < 20; ++x) {
        for (unsigned int y = 0; y < 20; ++y) {
            if (x < 2 || x > 17 || y < 2 || y > 17) {
                whiteImg.setPixel(x, y, sf::Color::Black);
                blackImg.setPixel(x, y, sf::Color::White);
            }
        }
    }
    
    fallbackWhite.loadFromImage(whiteImg);
    fallbackBlack.loadFromImage(blackImg);
    
    // Load each texture
    for (const auto& piece : pieceNames) {
        // Try multiple paths for each piece
        std::vector<std::string> possiblePaths = {
            piecesDir.string() + "/" + piece + ".png",
            "./pieces/" + piece + ".png",
            "../pieces/" + piece + ".png",
            "../../pieces/" + piece + ".png",
            "build/exe/pieces/" + piece + ".png"
        };
        
        bool loaded = false;
        for (const auto& filename : possiblePaths) {
            if (std::filesystem::exists(filename)) {
                sf::Texture texture;
                if (texture.loadFromFile(filename)) {
                    texture.setSmooth(true); // Enable smooth scaling
                    textures[piece] = std::move(texture);
                    loaded = true;
                    anyLoaded = true;
                    break;
                }
            }
        }
        
        if (!loaded) {
            std::cerr << "Texture file not found for: " << piece << std::endl;
            // Use fallback texture
            if (piece.find("white") != std::string::npos) {
                textures[piece] = fallbackWhite;
            } else {
                textures[piece] = fallbackBlack;
            }
            allLoaded = false;
        }
    }
    
    // If no pieces were loaded but we created fallbacks, we can still continue
    return anyLoaded || !allLoaded;
}

// Fast lookup mapping of FEN characters to piece types - using unordered_map for better performance
const std::unordered_map<char, PieceType> FEN_TO_PIECE_TYPE = {
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

// Get piece color from FEN character - inline for performance
PieceColor getColorFromFEN(char fenChar) {
    return std::isupper(fenChar) ? PieceColor::WHITE : PieceColor::BLACK;
}

// Get piece texture key from FEN character - optimized using static map
std::string getTextureKeyFromFEN(char fenChar) {
    static const std::unordered_map<char, std::string> pieceMap = {
        {'p', "pawn"}, {'r', "rook"}, {'n', "knight"}, 
        {'b', "bishop"}, {'q', "queen"}, {'k', "king"}
    };
    
    std::string color = std::isupper(fenChar) ? "white" : "black";
    char lowerChar = std::tolower(fenChar);
    
    auto it = pieceMap.find(lowerChar);
    if (it != pieceMap.end()) {
        return color + "-" + it->second;
    }
    
    return ""; // Invalid character
}

// Function to set up pieces according to FEN notation
void setupPositionFromFEN(std::map<std::pair<int, int>, ChessPiece>& pieces, const std::string& fen) {
    pieces.clear();
    constexpr float SQUARE_SIZE = 100.0f;
    auto& textureManager = PieceTextureManager::getInstance();
    
    // Parse board position (first part of FEN)
    std::string boardFEN;
    size_t spacePos = fen.find(' ');
    if (spacePos != std::string::npos) {
        boardFEN = fen.substr(0, spacePos);
    } else {
        boardFEN = fen; // Use the whole string if no space found
    }
    
    int row = 0;
    int col = 0;
    
    // Pre-check for valid FEN string (should have 8 rows)
    int rowCount = 1; // Start with 1 because we count separators
    for (char c : boardFEN) {
        if (c == '/') rowCount++;
    }
    
    if (rowCount != 8) {
        std::cerr << "Warning: FEN string does not have 8 rows. It has " << rowCount << " rows." << std::endl;
        // Continue anyway - we'll handle this gracefully
    }
    
    auto charIt = boardFEN.begin();
    auto endIt = boardFEN.end();
    
    // Row and column bounds checking
    while (charIt != endIt && row < 8) {
        char c = *charIt++;
        
        if (c == '/') {
            // Move to next row, reset column
            row++;
            col = 0;
            
            // Skip invalid rows
            if (row >= 8) break;
        } else if (std::isdigit(c)) {
            // Skip empty squares
            int emptyCount = c - '0';
            
            // Check for invalid column bounds
            if (col + emptyCount > 8) {
                std::cerr << "Warning: FEN string specifies too many columns in row " << row + 1 << std::endl;
                // Clamp to valid range
                emptyCount = 8 - col;
            }
            
            col += emptyCount;
        } else if (std::isalpha(c)) {
            // Place a piece (only if within bounds)
            if (col < 8) {
                // Check if valid piece character
                auto fenIt = FEN_TO_PIECE_TYPE.find(c);
                if (fenIt == FEN_TO_PIECE_TYPE.end()) {
                    std::cerr << "Warning: Invalid piece character '" << c << "' in FEN string" << std::endl;
                    col++; // Skip this character
                    continue;
                }
                
                std::string textureKey = getTextureKeyFromFEN(c);
                const sf::Texture* texture = textureManager.getTexture(textureKey);
                
                if (!textureKey.empty()) {
                    ChessPiece piece{
                        fenIt->second,
                        getColorFromFEN(c),
                        sf::Sprite()  // Initialize with empty sprite in case texture is missing
                    };
                    
                    // Only set up sprite if texture is available
                    if (texture) {
                        piece.sprite = sf::Sprite(*texture);
                        
                        // Direct calculation of scaling and positioning for performance
                        float textureWidth = texture->getSize().x;
                        float textureHeight = texture->getSize().y;
                        float scaleFactor = textureManager.getScale();
                        float scaleX = (SQUARE_SIZE / textureWidth) * scaleFactor;
                        float scaleY = (SQUARE_SIZE / textureHeight) * scaleFactor;
                        
                        piece.sprite.setScale(scaleX, scaleY);
                        
                        // Position the piece with center alignment
                        float offsetX = (SQUARE_SIZE - (textureWidth * scaleX)) / 2;
                        float offsetY = (SQUARE_SIZE - (textureHeight * scaleY)) / 2;
                        
                        piece.sprite.setPosition(
                            col * SQUARE_SIZE + offsetX,
                            row * SQUARE_SIZE + offsetY
                        );
                    }
                    
                    pieces[{col, row}] = std::move(piece);
                }
                
                col++;
            } else {
                std::cerr << "Warning: FEN string has too many pieces in row " << row + 1 << std::endl;
                // Skip to next row
                while (charIt != endIt && *charIt != '/') charIt++;
                if (charIt != endIt) charIt++; // Skip the '/'
                row++;
                col = 0;
            }
        } else {
            // Skip invalid characters
            std::cerr << "Warning: Invalid character '" << c << "' in FEN string" << std::endl;
        }
    }
}

// Function to draw pieces on the board
void drawPieces(sf::RenderWindow& window, const std::map<std::pair<int, int>, ChessPiece>& pieces) {
    for (const auto& [position, piece] : pieces) {
        window.draw(piece.sprite);
    }
} 