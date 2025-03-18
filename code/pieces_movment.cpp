#include "pieces_movment.h"
#include <iostream>

ChessInteraction::ChessInteraction(std::map<BoardPosition, ChessPiece>& piecesRef, float squareSz)
    : pieces(piecesRef), squareSize(squareSz),
      whiteKingMoved(false), blackKingMoved(false),
      whiteKingsideRookMoved(false), whiteQueensideRookMoved(false),
      blackKingsideRookMoved(false), blackQueensideRookMoved(false)
{
    // Initialize promotion panel
    const float panelWidth = squareSize * 1.5f;
    const float headerHeight = squareSize * 0.4f;
    const float panelHeight = 4 * squareSize + headerHeight;
    const float borderThickness = 3.0f;
    
    // Preallocate UI elements with optimized initialization
    promotionPanel.setSize(sf::Vector2f(panelWidth, panelHeight));
    promotionPanel.setFillColor(promotionPanelColor);
    
    promotionHeader.setSize(sf::Vector2f(panelWidth, headerHeight));
    promotionHeader.setFillColor(promotionHeaderColor);
    
    promotionBorder.setSize(sf::Vector2f(panelWidth + 2*borderThickness, panelHeight + 2*borderThickness));
    promotionBorder.setFillColor(promotionBorderColor);
    
    // Preallocate promotion highlights
    promotionSelectionHighlights.reserve(4);
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape highlight;
        highlight.setSize(sf::Vector2f(panelWidth - 10, squareSize - 10));
        highlight.setFillColor(sf::Color(173, 216, 230, 120));
        highlight.setOutlineThickness(2);
        highlight.setOutlineColor(sf::Color(100, 149, 237));
        promotionSelectionHighlights.push_back(std::move(highlight));
    }
}

bool ChessInteraction::handleMouseClick(int x, int y) {
    // Cache the old selection for comparison
    auto oldSelection = selectedSquare;
    
    // Handle promotion selection
    if (awaitingPromotion) {
        // Get promotion panel bounds
        const float panelX = promotionPanel.getPosition().x;
        const float panelY = promotionPanel.getPosition().y;
        const float panelWidth = promotionPanel.getSize().x;
        const float panelHeight = promotionPanel.getSize().y;
        const float headerHeight = promotionHeader.getSize().y;
        
        // Check if click is within promotion panel
        if (x >= panelX && x <= panelX + panelWidth &&
            y >= panelY + headerHeight && y <= panelY + panelHeight) {
            
            // Calculate which option was clicked
            const int relativeY = y - (panelY + headerHeight);
            const int optionIndex = relativeY / static_cast<int>(squareSize);
            
            // Process valid selection
            if (optionIndex >= 0 && optionIndex < 4) {
                static const PieceType promotionTypes[4] = {
                    PieceType::QUEEN, PieceType::ROOK, 
                    PieceType::BISHOP, PieceType::KNIGHT
                };
                
                executePromotion(promotionTypes[optionIndex]);
                awaitingPromotion = false;
                return true;
            }
        } else {
            // Click outside the promotion panel - cancel promotion
            awaitingPromotion = false;
            return true;
        }
    }
    
    // Convert mouse position to board coordinates
    const int file = x / static_cast<int>(squareSize);
    const int rank = y / static_cast<int>(squareSize);
    
    // Ensure we're within board boundaries
    if (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
        const BoardPosition clickedSquare = {file, rank};
        
        // Check if we clicked on a legal move square
        if (selectedSquare.first != -1) {
            if (std::find(legalMoves.begin(), legalMoves.end(), clickedSquare) != legalMoves.end()) {
                // Execute the move with all special case handling
                movePiece(selectedSquare, clickedSquare);
                
                // Clear selection after move (unless we're awaiting promotion)
                if (!awaitingPromotion) {
                    clearSelection();
                }
                return true;
            }
        }
        
        // Check if there's a piece on the clicked square
        auto pieceIt = pieces.find(clickedSquare);
        if (pieceIt != pieces.end()) {
            // If it's already selected, deselect it
            if (selectedSquare == clickedSquare) {
                clearSelection();
            } else {
                // Select the new square
                selectedSquare = clickedSquare;
                
                // Reset highlight animation when selecting a new piece
                selectionAlpha = MAX_ALPHA;
                selectionPulseDir = -1.0f;
                
                // Calculate legal moves for the selected piece
                calculateLegalMoves();
            }
        } else {
            // Clicked on an empty square, deselect
            clearSelection();
        }
    }
    
    // Return true if selection changed
    return oldSelection != selectedSquare;
}

void ChessInteraction::movePiece(const BoardPosition& from, const BoardPosition& to) {
    auto pieceIt = pieces.find(from);
    if (pieceIt == pieces.end()) return;
    
    ChessPiece movingPiece = pieceIt->second;
    
    // Update castling flags
    if (movingPiece.type == PieceType::KING) {
        if (movingPiece.color == PieceColor::WHITE) {
            whiteKingMoved = true;
        } else {
            blackKingMoved = true;
        }
    } else if (movingPiece.type == PieceType::ROOK) {
        const int y = from.second;
        const int x = from.first;
        
        // Optimized rook tracking using a single if with compound conditions
        if (movingPiece.color == PieceColor::WHITE && y == 7) {
            if (x == 0) whiteQueensideRookMoved = true;
            else if (x == 7) whiteKingsideRookMoved = true;
        } else if (movingPiece.color == PieceColor::BLACK && y == 0) {
            if (x == 0) blackQueensideRookMoved = true;
            else if (x == 7) blackKingsideRookMoved = true;
        }
    }
    
    // Handle castling with optimized code
    if (isCastlingMove(from, to)) {
        const int y = from.second;
        BoardPosition rookFrom, rookTo;
        
        if (to.first > from.first) { // Kingside castling
            rookFrom = {7, y};
            rookTo = {to.first - 1, y}; // f1 for white, f8 for black
        } else { // Queenside castling
            rookFrom = {0, y};
            rookTo = {to.first + 1, y}; // d1 for white, d8 for black
        }
        
        // Move the rook
        auto rookIt = pieces.find(rookFrom);
        if (rookIt != pieces.end()) {
            // Create a copy of the rook for moving
            ChessPiece rook = rookIt->second;
            
            // Position the rook sprite properly
            const sf::Texture* texture = rook.sprite.getTexture();
            if (texture) {
                const float textureWidth = texture->getSize().x;
                const float textureHeight = texture->getSize().y;
                const float scaleX = rook.sprite.getScale().x;
                const float scaleY = rook.sprite.getScale().y;
                
                // Center the rook in its new square
                const float offsetX = (squareSize - (textureWidth * scaleX)) / 2;
                const float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
                
                rook.sprite.setPosition(
                    rookTo.first * squareSize + offsetX,
                    rookTo.second * squareSize + offsetY
                );
            }
            
            // Remove rook from old position and place at new position
            pieces.erase(rookFrom);
            pieces[rookTo] = std::move(rook);
        }
    }
    
    // Check for promotion
    if (isPromotionMove(from, to)) {
        // Save the move info and show promotion options
        showPromotionOptions(to, movingPiece.color);
        
        // Center the pawn in the destination square properly
        const sf::Texture* texture = movingPiece.sprite.getTexture();
        if (texture) {
            const float textureWidth = texture->getSize().x;
            const float textureHeight = texture->getSize().y;
            const float scaleX = movingPiece.sprite.getScale().x;
            const float scaleY = movingPiece.sprite.getScale().y;
            
            // Calculate position to center the piece in the square
            const float offsetX = (squareSize - (textureWidth * scaleX)) / 2;
            const float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
            
            movingPiece.sprite.setPosition(
                to.first * squareSize + offsetX,
                to.second * squareSize + offsetY
            );
        } else {
            // Fallback if texture not available
            movingPiece.sprite.setPosition(to.first * squareSize, to.second * squareSize);
        }
        
        // Remove any captured piece at the destination
        pieces.erase(to);
        
        // Move pawn to destination temporarily
        pieces.erase(from);
        pieces[to] = std::move(movingPiece);
        
        // Set flag to await promotion choice
        awaitingPromotion = true;
        promotionSquare = to;
        promotionColor = movingPiece.color;
        
        return;
    }
    
    // Check for en passant capture
    if (isEnPassantMove(from, to)) {
        // Remove the captured pawn
        BoardPosition capturedPawnPos = {to.first, from.second};
        pieces.erase(capturedPawnPos);
    }
    
    // Capture any piece at destination
    pieces.erase(to);
    
    // Center the piece properly in its destination square
    const sf::Texture* texture = movingPiece.sprite.getTexture();
    if (texture) {
        const float textureWidth = texture->getSize().x;
        const float textureHeight = texture->getSize().y;
        const float scaleX = movingPiece.sprite.getScale().x;
        const float scaleY = movingPiece.sprite.getScale().y;
        
        // Calculate position to center the piece in the square
        const float offsetX = (squareSize - (textureWidth * scaleX)) / 2;
        const float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
        
        movingPiece.sprite.setPosition(
            to.first * squareSize + offsetX,
            to.second * squareSize + offsetY
        );
    } else {
        // Fallback if texture not available
        movingPiece.sprite.setPosition(to.first * squareSize, to.second * squareSize);
    }
    
    // Move the piece in the data structure
    pieces.erase(from);
    pieces[to] = std::move(movingPiece);
    
    // Update en passant target
    if (movingPiece.type == PieceType::PAWN && std::abs(to.second - from.second) == 2) {
        // Set en passant target square
        enPassantTarget = {to.first, (from.second + to.second) / 2};
    } else {
        // Clear en passant target
        enPassantTarget = {-1, -1};
    }
}

bool ChessInteraction::isPromotionMove(const BoardPosition& from, const BoardPosition& to) const {
    auto pieceIt = pieces.find(from);
    if (pieceIt == pieces.end()) return false;
    
    const ChessPiece& piece = pieceIt->second;
    
    // Quick check first for performance
    if (piece.type != PieceType::PAWN) return false;
    
    // Check if it's reaching the opposite end
    return (piece.color == PieceColor::WHITE && to.second == 0) ||
           (piece.color == PieceColor::BLACK && to.second == 7);
}

bool ChessInteraction::isEnPassantMove(const BoardPosition& from, const BoardPosition& to) const {
    // If there's no en passant target, return false
    if (enPassantTarget.first == -1) return false;
    
    auto pieceIt = pieces.find(from);
    if (pieceIt == pieces.end()) return false;
    
    const ChessPiece& piece = pieceIt->second;
    
    // Quick check first for performance
    if (piece.type != PieceType::PAWN) return false;
    
    // Check if destination is the en passant target and it's a diagonal move
    return to == enPassantTarget && 
           std::abs(to.first - from.first) == 1 && 
           std::abs(to.second - from.second) == 1;
}

void ChessInteraction::showPromotionOptions(const BoardPosition& square, PieceColor color) {
    promotionOptions.clear();
    
    // Define the pieces to choose from (in order: Queen, Rook, Bishop, Knight)
    static const PieceType promotionTypes[] = {
        PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT
    };
    
    // Calculate optimal position for the promotion panel
    const float panelWidth = promotionPanel.getSize().x;
    const float panelHeight = promotionPanel.getSize().y;
    const float headerHeight = promotionHeader.getSize().y;
    const float borderThickness = 3.0f;
    
    // Position the panel intelligently based on available space
    float panelX = (square.first < 6) ? (square.first + 1.2f) * squareSize : 
                                       (square.first - 1.7f) * squareSize;
    
    // Position vertically to be centered
    float panelY = std::max(0.0f, std::min(
        (8.0f * squareSize) - panelHeight,  // Don't go below bottom
        square.second * squareSize - (panelHeight / 2) + (squareSize / 2)  // Center on square
    ));
    
    // Set positions
    promotionBorder.setPosition(panelX - borderThickness, panelY - borderThickness);
    promotionPanel.setPosition(panelX, panelY);
    promotionHeader.setPosition(panelX, panelY);
    
    // Get texture manager reference
    auto& textureManager = PieceTextureManager::getInstance();
    
    // String buffers for pieces (avoids recreation in loop)
    std::string colorName = (color == PieceColor::WHITE) ? "white" : "black";
    std::string textureKey;
    
    // Create sprite for each promotion option
    for (int i = 0; i < 4; i++) {
        // Construct the texture key
        switch (promotionTypes[i]) {
            case PieceType::QUEEN: textureKey = colorName + "-queen"; break;
            case PieceType::ROOK: textureKey = colorName + "-rook"; break;
            case PieceType::BISHOP: textureKey = colorName + "-bishop"; break;
            case PieceType::KNIGHT: textureKey = colorName + "-knight"; break;
            default: continue;
        }
        
        const sf::Texture* texture = textureManager.getTexture(textureKey);
        
        if (texture) {
            ChessPiece option{promotionTypes[i], color, sf::Sprite(*texture)};
            
            // Scale the sprite
            const float textureWidth = texture->getSize().x;
            const float textureHeight = texture->getSize().y;
            const float scaleFactor = textureManager.getScale() * 0.9f; // Slightly smaller
            const float scaleX = (squareSize / textureWidth) * scaleFactor;
            const float scaleY = (squareSize / textureHeight) * scaleFactor;
            option.sprite.setScale(scaleX, scaleY);
            
            // Center the piece in its panel slot
            const float offsetX = (panelWidth - (textureWidth * scaleX)) / 2;
            const float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
            
            const float optionY = panelY + headerHeight + (i * squareSize);
            option.sprite.setPosition(panelX + offsetX, optionY + offsetY);
            
            // Position the selection highlight
            promotionSelectionHighlights[i].setPosition(panelX + 5, optionY + 5);
            
            promotionOptions.push_back(std::move(option));
        }
    }
}

void ChessInteraction::executePromotion(PieceType promotionType) {
    auto pieceIt = pieces.find(promotionSquare);
    if (pieceIt == pieces.end()) return;
    
    // Create the new promoted piece
    std::string colorName = (promotionColor == PieceColor::WHITE) ? "white" : "black";
    std::string typeName;
    
    switch (promotionType) {
        case PieceType::QUEEN: typeName = "queen"; break;
        case PieceType::ROOK: typeName = "rook"; break;
        case PieceType::BISHOP: typeName = "bishop"; break;
        case PieceType::KNIGHT: typeName = "knight"; break;
        default: typeName = "queen"; break;
    }
    
    std::string textureKey = colorName + "-" + typeName;
    const sf::Texture* texture = PieceTextureManager::getInstance().getTexture(textureKey);
    
    if (!texture) return;
    
    // Create the promoted piece
    ChessPiece promotedPiece{
        promotionType,
        promotionColor,
        sf::Sprite(*texture)
    };
    
    // Set appropriate scale and position
    const float textureWidth = texture->getSize().x;
    const float textureHeight = texture->getSize().y;
    const float scaleFactor = PieceTextureManager::getInstance().getScale();
    const float scaleX = (squareSize / textureWidth) * scaleFactor;
    const float scaleY = (squareSize / textureHeight) * scaleFactor;
    promotedPiece.sprite.setScale(scaleX, scaleY);
    
    // Center the piece in its square
    const float offsetX = (squareSize - (textureWidth * scaleX)) / 2;
    const float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
    
    promotedPiece.sprite.setPosition(
        promotionSquare.first * squareSize + offsetX,
        promotionSquare.second * squareSize + offsetY
    );
    
    // Replace the pawn with the promoted piece
    pieces[promotionSquare] = std::move(promotedPiece);
}

void ChessInteraction::calculateLegalMoves() {
    // Clear and preallocate for performance
    legalMoves.clear();
    legalMoves.reserve(32); // Reasonable estimate for max legal moves
    
    // If no piece is selected, do nothing
    if (selectedSquare.first == -1) return;
    
    // Get the selected piece
    auto pieceIt = pieces.find(selectedSquare);
    if (pieceIt == pieces.end()) return;
    
    const ChessPiece& piece = pieceIt->second;
    
    // Calculate legal moves based on piece type
    switch (piece.type) {
        case PieceType::PAWN:
            legalMoves = getPawnMoves(selectedSquare, piece);
            break;
        case PieceType::ROOK:
            legalMoves = getRookMoves(selectedSquare, piece);
            break;
        case PieceType::KNIGHT:
            legalMoves = getKnightMoves(selectedSquare, piece);
            break;
        case PieceType::BISHOP:
            legalMoves = getBishopMoves(selectedSquare, piece);
            break;
        case PieceType::QUEEN:
            legalMoves = getQueenMoves(selectedSquare, piece);
            break;
        case PieceType::KING:
            legalMoves = getKingMoves(selectedSquare, piece);
            break;
    }
}

std::vector<BoardPosition> ChessInteraction::getPawnMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    moves.reserve(4); // Maximum possible pawn moves
    
    const int x = pos.first;
    const int y = pos.second;
    
    // Determine direction (white pawns move up the board, black pawns down)
    const int direction = (piece.color == PieceColor::WHITE) ? -1 : 1;
    
    // Forward move (can't capture)
    BoardPosition forwardPos{x, y + direction};
    if (forwardPos.second >= 0 && forwardPos.second < 8) {
        // Check if the square is empty
        if (pieces.find(forwardPos) == pieces.end()) {
            moves.push_back(forwardPos);
            
            // If pawn is in starting position, it can move two squares forward
            const bool inStartingPos = (piece.color == PieceColor::WHITE && y == 6) || 
                                      (piece.color == PieceColor::BLACK && y == 1);
                                
            if (inStartingPos) {
                BoardPosition doubleForwardPos{x, y + 2 * direction};
                // Check if both squares are empty
                if (pieces.find(doubleForwardPos) == pieces.end()) {
                    moves.push_back(doubleForwardPos);
                }
            }
        }
    }
    
    // Diagonal captures - optimized using array instead of initializer list
    static const int dxValues[2] = {-1, 1};
    for (int dx : dxValues) {
        BoardPosition capturePos{x + dx, y + direction};
        // Make sure position is on the board
        if (capturePos.first >= 0 && capturePos.first < 8 && 
            capturePos.second >= 0 && capturePos.second < 8) {
            
            // Check if there's an enemy piece
            auto it = pieces.find(capturePos);
            if (it != pieces.end() && it->second.color != piece.color) {
                moves.push_back(capturePos);
            }
            
            // Check en passant capture
            if (capturePos == enPassantTarget) {
                moves.push_back(capturePos);
            }
        }
    }
    
    return moves;
}

std::vector<BoardPosition> ChessInteraction::getRookMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    moves.reserve(14); // Rooks can move to at most 14 squares
    
    // Define the four directions as a static array for optimization
    static const std::pair<int, int> directions[4] = {
        {0, -1}, {1, 0}, {0, 1}, {-1, 0}
    };
    
    // Check moves in all four directions
    for (const auto& [dx, dy] : directions) {
        addMovesInDirection(moves, pos, dx, dy, piece);
    }
    
    return moves;
}

std::vector<BoardPosition> ChessInteraction::getKnightMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    moves.reserve(8); // Knights can move to at most 8 squares
    
    const int x = pos.first;
    const int y = pos.second;
    
    // All eight possible knight moves
    static const std::pair<int, int> knightOffsets[8] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (const auto& [dx, dy] : knightOffsets) {
        const int newX = x + dx;
        const int newY = y + dy;
        
        // Check if the new position is on the board
        if (newX >= 0 && newX < 8 && newY >= 0 && newY < 8) {
            BoardPosition newPos{newX, newY};
            
            // Check if the square is empty or has an enemy piece
            auto it = pieces.find(newPos);
            if (it == pieces.end() || it->second.color != piece.color) {
                moves.push_back(newPos);
            }
        }
    }
    
    return moves;
}

std::vector<BoardPosition> ChessInteraction::getBishopMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    moves.reserve(13); // Bishops can move to at most 13 squares
    
    // Define the four directions as a static array for optimization
    static const std::pair<int, int> directions[4] = {
        {1, -1}, {1, 1}, {-1, 1}, {-1, -1}
    };
    
    // Check moves in all four diagonal directions
    for (const auto& [dx, dy] : directions) {
        addMovesInDirection(moves, pos, dx, dy, piece);
    }
    
    return moves;
}

std::vector<BoardPosition> ChessInteraction::getQueenMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    moves.reserve(27); // Queens can move to at most 27 squares
    
    // Define all eight directions as a static array for optimization
    static const std::pair<int, int> directions[8] = {
        {0, -1}, {1, -1}, {1, 0}, {1, 1}, 
        {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
    };
    
    // Check moves in all eight directions
    for (const auto& [dx, dy] : directions) {
        addMovesInDirection(moves, pos, dx, dy, piece);
    }
    
    return moves;
}

void ChessInteraction::addMovesInDirection(std::vector<BoardPosition>& moves, 
                                          const BoardPosition& startPos, 
                                          int dirX, int dirY, 
                                          const ChessPiece& piece) {
    const int x = startPos.first;
    const int y = startPos.second;
    
    for (int i = 1; i < 8; ++i) {
        const int newX = x + i * dirX;
        const int newY = y + i * dirY;
        
        // Check if the new position is on the board
        if (newX < 0 || newX >= 8 || newY < 0 || newY >= 8) {
            break; // Off the board
        }
        
        BoardPosition newPos{newX, newY};
        auto it = pieces.find(newPos);
        
        if (it == pieces.end()) {
            // Empty square, we can move here
            moves.push_back(newPos);
        } else {
            // Square has a piece
            if (it->second.color != piece.color) {
                // Enemy piece, we can capture it
                moves.push_back(newPos);
            }
            // We can't move further in this direction
            break;
        }
    }
}

// New method for king moves
std::vector<BoardPosition> ChessInteraction::getKingMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    moves.reserve(10); // Kings can move to at most 8 squares + 2 castling moves
    
    const int x = pos.first;
    const int y = pos.second;
    
    // Define all eight directions as a static array for optimization
    static const std::pair<int, int> directions[8] = {
        {-1, -1}, {0, -1}, {1, -1}, {1, 0}, 
        {1, 1}, {0, 1}, {-1, 1}, {-1, 0}
    };
    
    // Regular king moves (one square in any direction)
    for (const auto& [dx, dy] : directions) {
        const int newX = x + dx;
        const int newY = y + dy;
        
        // Check if the new position is on the board
        if (newX >= 0 && newX < 8 && newY >= 0 && newY < 8) {
            BoardPosition newPos{newX, newY};
            
            // Check if the square is empty or has an enemy piece
            auto it = pieces.find(newPos);
            if (it == pieces.end() || it->second.color != piece.color) {
                moves.push_back(newPos);
            }
        }
    }
    
    // Check for castling possibilities
    if (piece.color == PieceColor::WHITE) {
        if (!whiteKingMoved) {
            // Kingside castling
            if (!whiteKingsideRookMoved && canCastle(pos, 1, piece)) {
                moves.push_back({x + 2, y});
            }
            
            // Queenside castling
            if (!whiteQueensideRookMoved && canCastle(pos, -1, piece)) {
                moves.push_back({x - 2, y});
            }
        }
    } else {
        if (!blackKingMoved) {
            // Kingside castling
            if (!blackKingsideRookMoved && canCastle(pos, 1, piece)) {
                moves.push_back({x + 2, y});
            }
            
            // Queenside castling
            if (!blackQueensideRookMoved && canCastle(pos, -1, piece)) {
                moves.push_back({x - 2, y});
            }
        }
    }
    
    return moves;
}

bool ChessInteraction::canCastle(const BoardPosition& kingPos, int direction, const ChessPiece& king) {
    const int x = kingPos.first;
    const int y = kingPos.second;
    
    // Quick check for king position
    if ((king.color == PieceColor::WHITE && y != 7) ||
        (king.color == PieceColor::BLACK && y != 0)) {
        return false;
    }
    
    // Determine rook position
    const BoardPosition rookPos{direction > 0 ? 7 : 0, y};
    
    // Check that the rook is present and correct type/color
    auto rookIt = pieces.find(rookPos);
    if (rookIt == pieces.end() || 
        rookIt->second.type != PieceType::ROOK || 
        rookIt->second.color != king.color) {
        return false;
    }
    
    // Check that squares between king and rook are empty
    const int startCol = x + direction;
    const int endCol = direction > 0 ? rookPos.first - 1 : rookPos.first + 1;
    
    for (int col = std::min(startCol, endCol); col <= std::max(startCol, endCol); col++) {
        if (pieces.find({col, y}) != pieces.end()) {
            return false; // There's a piece in between
        }
    }
    
    return true;
}

bool ChessInteraction::isCastlingMove(const BoardPosition& from, const BoardPosition& to) const {
    auto pieceIt = pieces.find(from);
    return pieceIt != pieces.end() && 
           pieceIt->second.type == PieceType::KING && 
           from.second == to.second && 
           std::abs(to.first - from.first) == 2;
}

void ChessInteraction::update(float deltaTime) {
    // Only animate if something is selected
    if (selectedSquare.first != -1 && selectedSquare.second != -1) {
        // Pulse the highlight effect
        selectionAlpha += selectionPulseDir * PULSE_SPEED * deltaTime;
        
        // Change direction at bounds
        if (selectionAlpha <= MIN_ALPHA) {
            selectionAlpha = MIN_ALPHA;
            selectionPulseDir = 1.0f;
        } else if (selectionAlpha >= MAX_ALPHA) {
            selectionAlpha = MAX_ALPHA;
            selectionPulseDir = -1.0f;
        }
        
        // Update the current highlight color with new alpha
        currentSelectionColor = sf::Color(
            baseSelectionColor.r,
            baseSelectionColor.g,
            baseSelectionColor.b,
            static_cast<sf::Uint8>(selectionAlpha)
        );
    }
    
    // Update promotion panel hover effects - this would require mouse position
    // but we'll skip it since we don't have access to the window here
    if (awaitingPromotion) {
        // Set all highlights to default state
        for (auto& highlight : promotionSelectionHighlights) {
            highlight.setFillColor(sf::Color(173, 216, 230, 60));
        }
    }
}

void ChessInteraction::draw(sf::RenderWindow& window) {
    // Draw legal move indicators as highlighted squares
    for (const auto& pos : legalMoves) {
        sf::RectangleShape moveHighlight(sf::Vector2f(squareSize, squareSize));
        moveHighlight.setPosition(pos.first * squareSize, pos.second * squareSize);
        moveHighlight.setFillColor(legalMoveColor);
        window.draw(moveHighlight);
    }
    
    // Draw highlight for selected square if any
    if (selectedSquare.first != -1 && selectedSquare.second != -1) {
        sf::RectangleShape highlight(sf::Vector2f(squareSize, squareSize));
        highlight.setPosition(selectedSquare.first * squareSize, selectedSquare.second * squareSize);
        highlight.setFillColor(currentSelectionColor);
        window.draw(highlight);
    }
    
    // Draw promotion UI if active
    if (awaitingPromotion) {
        // Create a semi-transparent overlay to dim the entire board
        sf::RectangleShape overlay(sf::Vector2f(8 * squareSize, 8 * squareSize));
        overlay.setFillColor(sf::Color(0, 0, 0, 100)); // Semi-transparent black
        window.draw(overlay);
        
        // Draw border
        window.draw(promotionBorder);
        
        // Draw main panel
        window.draw(promotionPanel);
        
        // Draw header
        window.draw(promotionHeader);
        
        // Draw selection highlights
        for (size_t i = 0; i < std::min(promotionOptions.size(), static_cast<size_t>(4)); i++) {
            window.draw(promotionSelectionHighlights[i]);
        }
        
        // Draw promotion piece options
        for (const auto& option : promotionOptions) {
            window.draw(option.sprite);
        }
    }
}