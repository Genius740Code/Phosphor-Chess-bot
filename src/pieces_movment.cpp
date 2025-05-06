#include "pieces_movment.h"
#include <iostream>

ChessInteraction::ChessInteraction(std::map<BoardPosition, ChessPiece>& piecesRef, 
                                  ChessGameLogic& gameLogicRef,
                                  float squareSz)
    : pieces(piecesRef), gameLogic(gameLogicRef), squareSize(squareSz),
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
    // Game is over - disable moves
    if (gameLogic.getGameState() != GameState::ACTIVE && 
        gameLogic.getGameState() != GameState::CHECK) {
        return false;
    }

    // Get current player and check state
    PieceColor currentTurn = gameLogic.getCurrentTurn();
    bool isInCheck = (gameLogic.getGameState() == GameState::CHECK);
    
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
                
                // After promotion, switch turns
                gameLogic.switchTurn();
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
        
        // Check if we clicked on a legal move square (with a piece selected)
        if (selectedSquare.first != -1) {
            if (std::find(legalMoves.begin(), legalMoves.end(), clickedSquare) != legalMoves.end()) {
                // Execute the move with all special case handling
                movePiece(selectedSquare, clickedSquare);
                
                // Clear selection after move (unless we're awaiting promotion)
                if (!awaitingPromotion) {
                    clearSelection();
                    
                    // Switch turns
                    gameLogic.switchTurn();
                    
                    // If player was in check, verify the move resolved the check
                    if (isInCheck && gameLogic.getGameState() == GameState::CHECK) {
                        // The move didn't resolve check - this shouldn't happen with proper legal move generation
                        // But as a fallback, we'll revert to the previous turn
                        gameLogic.switchTurn(); // Switch back
                        return false;
                    }
                }
                return true;
            }
        }
        
        // Check if there's a piece on the clicked square
        auto pieceIt = pieces.find(clickedSquare);
        if (pieceIt != pieces.end()) {
            // Make sure player can only select their own pieces
            if (pieceIt->second.color != currentTurn) {
                // Not your turn - can't select opponent's pieces
                return false;
            }
            
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
                
                // If in check, filter moves to only those that resolve the check
                if (isInCheck && !legalMoves.empty()) {
                    // Cache the king's position
                    BoardPosition kingPos = (currentTurn == PieceColor::WHITE) ? 
                                            gameLogic.getKingPosition(PieceColor::WHITE) : 
                                            gameLogic.getKingPosition(PieceColor::BLACK);
                    
                    // Filter moves to only those that resolve check
                    auto newLegalMoves = legalMoves;
                    legalMoves.clear();
                    
                    for (const auto& move : newLegalMoves) {
                        // Create a temporary board to simulate the move
                        auto tempPieces = pieces;
                        
                        // Special case if king is being moved
                        BoardPosition newKingPos = (pieceIt->second.type == PieceType::KING) ? move : kingPos;
                        
                        // Simulate the move
                        tempPieces[move] = tempPieces[selectedSquare];
                        tempPieces.erase(selectedSquare);
                        
                        // Check if king would still be in check
                        PieceColor opponentColor = (currentTurn == PieceColor::WHITE) ? 
                                                  PieceColor::BLACK : PieceColor::WHITE;
                        
                        // If this move would resolve check, add it to legal moves
                        if (!gameLogic.isSquareAttackedByPieces(newKingPos, opponentColor, tempPieces)) {
                            legalMoves.push_back(move);
                        }
                    }
                    
                    // If no legal moves left, deselect the piece
                    if (legalMoves.empty()) {
                        clearSelection();
                        return true;
                    }
                }
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
    // Get the moving piece
    ChessPiece movingPiece = pieces[from];
    PieceType pieceType = movingPiece.type;
    
    // Handle castling move
    if (pieceType == PieceType::KING && isCastlingMove(from, to)) {
        executeCastling(from, to);
        
        // Update the king moved flag for the appropriate color
        if (movingPiece.color == PieceColor::WHITE) {
            whiteKingMoved = true;
        } else {
            blackKingMoved = true;
        }
        
        // Update rook moved flags if necessary
        if (to.first > from.first) { // Kingside castling
            if (movingPiece.color == PieceColor::WHITE) {
                whiteKingsideRookMoved = true;
            } else {
                blackKingsideRookMoved = true;
            }
        } else { // Queenside castling
            if (movingPiece.color == PieceColor::WHITE) {
                whiteQueensideRookMoved = true;
            } else {
                blackQueensideRookMoved = true;
            }
        }
        
        return;
    }
    
    // Update castling flags for kings and rooks
    if (pieceType == PieceType::KING) {
        if (movingPiece.color == PieceColor::WHITE) {
            whiteKingMoved = true;
        } else {
            blackKingMoved = true;
        }
    } else if (pieceType == PieceType::ROOK) {
        if (from.first == 0 && from.second == 7) { // White queenside rook
            whiteQueensideRookMoved = true;
        } else if (from.first == 7 && from.second == 7) { // White kingside rook
            whiteKingsideRookMoved = true;
        } else if (from.first == 0 && from.second == 0) { // Black queenside rook
            blackQueensideRookMoved = true;
        } else if (from.first == 7 && from.second == 0) { // Black kingside rook
            blackKingsideRookMoved = true;
        }
    }
    
    // Check if this is a pawn promotion move
    if (pieceType == PieceType::PAWN && isPromotionMove(from, to)) {
        executePromotion(from, to);
        return;
    }
    
    // Execute the move
    pieces.erase(from);
    
    // Remove captured piece if any
    auto capturedIt = pieces.find(to);
    if (capturedIt != pieces.end()) {
        // Remove the captured piece
        pieces.erase(capturedIt);
    }
    
    // Place the moving piece in its new position
    pieces[to] = movingPiece;
    
    // Position the sprite properly in its new square
    positionPieceSprite(to);
}

// New helper method to position piece sprite correctly
void ChessInteraction::positionPieceSprite(const BoardPosition& pos) {
    auto pieceIt = pieces.find(pos);
    if (pieceIt == pieces.end()) return;
    
    const sf::Texture* texture = pieceIt->second.sprite.getTexture();
    if (!texture) return;
    
    const float textureWidth = texture->getSize().x;
    const float textureHeight = texture->getSize().y;
    const float scaleX = pieceIt->second.sprite.getScale().x;
    const float scaleY = pieceIt->second.sprite.getScale().y;
    
    const float offsetX = (squareSize - (textureWidth * scaleX)) / 2;
    const float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
    
    pieceIt->second.sprite.setPosition(
        pos.first * squareSize + offsetX,
        pos.second * squareSize + offsetY
    );
}

// New helper method to execute castling move
void ChessInteraction::executeCastling(const BoardPosition& from, const BoardPosition& to) {
    const int y = from.second;
    const int kingDirection = (to.first > from.first) ? 1 : -1;
    BoardPosition rookFrom, rookTo;
    
    if (kingDirection > 0) { // Kingside castling
        rookFrom = {7, y};
        rookTo = {to.first - 1, y}; // f1 for white, f8 for black
    } else { // Queenside castling
        rookFrom = {0, y};
        rookTo = {to.first + 1, y}; // d1 for white, d8 for black
    }
    
    // Move the king
    pieces[to] = pieces[from];
    pieces.erase(from);
    positionPieceSprite(to);
    
    // Move the rook
    auto rookIt = pieces.find(rookFrom);
    if (rookIt != pieces.end()) {
        pieces[rookTo] = rookIt->second;
        pieces.erase(rookFrom);
        positionPieceSprite(rookTo);
    }
}

// New helper method to handle promotion
void ChessInteraction::executePromotion(const BoardPosition& from, const BoardPosition& to) {
    // Move the piece to the new square (it will be replaced during promotion)
    pieces[to] = pieces[from];
    pieces.erase(from);
    
    // Position the sprite
    positionPieceSprite(to);
    
    // Set up promotion dialog
    showPromotionOptions(to, pieces[to].color);
    
    awaitingPromotion = true;
    promotionSquare = to;
    promotionColor = pieces[to].color;
}

// Implementation of the missing method - executePromotion with PieceType parameter
void ChessInteraction::executePromotion(PieceType promotionType) {
    // Make sure we have a valid promotion square
    if (promotionSquare.first == -1 || promotionSquare.second == -1) return;
    
    // Get texture manager reference
    auto& textureManager = PieceTextureManager::getInstance();
    
    // Construct the texture key for the promoted piece
    std::string colorName = (promotionColor == PieceColor::WHITE) ? "white" : "black";
    std::string pieceTypeName;
    
    switch (promotionType) {
        case PieceType::QUEEN: pieceTypeName = "queen"; break;
        case PieceType::ROOK: pieceTypeName = "rook"; break;
        case PieceType::BISHOP: pieceTypeName = "bishop"; break;
        case PieceType::KNIGHT: pieceTypeName = "knight"; break;
        default: return; // Invalid promotion type
    }
    
    std::string textureKey = colorName + "-" + pieceTypeName;
    const sf::Texture* texture = textureManager.getTexture(textureKey);
    
    if (texture) {
        // Replace the pawn with the new piece
        ChessPiece& piece = pieces[promotionSquare];
        piece.type = promotionType;
        piece.sprite.setTexture(*texture);
        
        // Rescale and reposition the sprite
        const float textureWidth = texture->getSize().x;
        const float textureHeight = texture->getSize().y;
        const float scaleFactor = textureManager.getScale();
        const float scaleX = (squareSize / textureWidth) * scaleFactor;
        const float scaleY = (squareSize / textureHeight) * scaleFactor;
        
        piece.sprite.setScale(scaleX, scaleY);
        
        // Position the piece with center alignment
        positionPieceSprite(promotionSquare);
    }
    
    // Reset promotion state
    promotionSquare = {-1, -1};
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

void ChessInteraction::calculateLegalMoves() {
    // Clear and preallocate for performance
    legalMoves.clear();
    
    // If no piece is selected, do nothing
    if (selectedSquare.first == -1) return;
    
    // Get the selected piece
    auto pieceIt = pieces.find(selectedSquare);
    if (pieceIt == pieces.end()) return;
    
    const ChessPiece& piece = pieceIt->second;
    
    // Ensure we're only calculating legal moves for the current player's pieces
    if (piece.color != gameLogic.getCurrentTurn()) return;
    
    // Use the game logic to get legal moves where possible
    std::vector<BoardPosition> potentialMoves = gameLogic.getLegalMoves(selectedSquare);
    
    // If we got moves from the game logic, use those
    if (!potentialMoves.empty()) {
        legalMoves = std::move(potentialMoves);
        return;
    }
    
    // Otherwise, fall back to our own calculation based on piece type
    // Pre-allocate based on piece type to avoid reallocations
    switch (piece.type) {
        case PieceType::PAWN:
            legalMoves.reserve(4); // Max possible pawn moves
            legalMoves = getPawnMoves(selectedSquare, piece);
            break;
        case PieceType::ROOK:
            legalMoves.reserve(14); // Rooks can move to at most 14 squares
            legalMoves = getRookMoves(selectedSquare, piece);
            break;
        case PieceType::KNIGHT:
            legalMoves.reserve(8); // Knights can move to at most 8 squares
            legalMoves = getKnightMoves(selectedSquare, piece);
            break;
        case PieceType::BISHOP:
            legalMoves.reserve(13); // Bishops can move to at most 13 squares
            legalMoves = getBishopMoves(selectedSquare, piece);
            break;
        case PieceType::QUEEN:
            legalMoves.reserve(27); // Queens can move to at most 27 squares
            legalMoves = getQueenMoves(selectedSquare, piece);
            break;
        case PieceType::KING:
            legalMoves.reserve(10); // Kings can move to at most 8 regular squares + 2 castling moves
            legalMoves = getKingMoves(selectedSquare, piece);
            break;
    }
    
    // If in check, we must ensure the moves actually get us out of check
    if (gameLogic.getGameState() == GameState::CHECK && !legalMoves.empty()) {
        // Cache the king's position
        BoardPosition kingPos = gameLogic.getKingPosition(piece.color);
        
        // Filter moves to only those that resolve check
        auto newLegalMoves = std::move(legalMoves);
        legalMoves.clear();
        legalMoves.reserve(newLegalMoves.size());
        
        PieceColor opponentColor = (piece.color == PieceColor::WHITE) ? 
                                  PieceColor::BLACK : PieceColor::WHITE;
        
        for (const auto& move : newLegalMoves) {
            // Create a temporary board to simulate the move
            auto tempPieces = pieces;
            
            // Special case if king is being moved
            BoardPosition newKingPos = (piece.type == PieceType::KING) ? move : kingPos;
            
            // Simulate the move
            tempPieces[move] = tempPieces[selectedSquare];
            tempPieces.erase(selectedSquare);
            
            // If this move would resolve check, add it to legal moves
            if (!gameLogic.isSquareAttackedByPieces(newKingPos, opponentColor, tempPieces)) {
                legalMoves.push_back(move);
            }
        }
    }
}

std::vector<BoardPosition> ChessInteraction::getPawnMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    moves.reserve(4); // Pawns can move to at most 4 squares
    
    const int x = pos.first;
    const int y = pos.second;
    
    // Fast check to avoid unnecessary calculations
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return moves;
    
    // Determine the direction of pawn movement based on color
    const int direction = (piece.color == PieceColor::WHITE) ? -1 : 1;
    
    // Forward move (one square)
    const int forwardY = y + direction;
    if (forwardY >= 0 && forwardY < 8) {
        BoardPosition forwardPos{x, forwardY};
        
        // Check if the square is empty
        if (pieces.find(forwardPos) == pieces.end()) {
            moves.push_back(forwardPos);
            
            // Forward move (two squares from starting position)
            const int startingRank = (piece.color == PieceColor::WHITE) ? 6 : 1;
            if (y == startingRank) {
                const int doubleForwardY = y + 2 * direction;
                if (doubleForwardY >= 0 && doubleForwardY < 8) {
                    BoardPosition doubleForwardPos{x, doubleForwardY};
                    
                    // Check if the square is empty
                    if (pieces.find(doubleForwardPos) == pieces.end()) {
                        moves.push_back(doubleForwardPos);
                    }
                }
            }
        }
    }
    
    // Capture moves (diagonally)
    for (int dx : {-1, 1}) {
        const int captureX = x + dx;
        const int captureY = y + direction;
        
        // Check if capture position is on the board
        if (captureX >= 0 && captureX < 8 && captureY >= 0 && captureY < 8) {
            BoardPosition capturePos{captureX, captureY};
            
            // Check if there's an enemy piece to capture
            auto it = pieces.find(capturePos);
            if (it != pieces.end() && it->second.color != piece.color) {
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
    
    // Fast check to avoid unnecessary calculations
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return moves;
    
    // All eight possible knight moves - use static array for performance
    static const std::pair<int, int> knightOffsets[8] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    // Pre-calculate enemy color
    const PieceColor enemyColor = (piece.color == PieceColor::WHITE) ? 
                                  PieceColor::BLACK : PieceColor::WHITE;
    
    for (const auto& [dx, dy] : knightOffsets) {
        const int newX = x + dx;
        const int newY = y + dy;
        
        // Check if the new position is on the board (use bounds checking)
        if (newX >= 0 && newX < 8 && newY >= 0 && newY < 8) {
            BoardPosition newPos{newX, newY};
            
            // Check if the square is empty or has an enemy piece
            auto it = pieces.find(newPos);
            if (it == pieces.end() || it->second.color == enemyColor) {
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

// Optimized method for king moves
std::vector<BoardPosition> ChessInteraction::getKingMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    moves.reserve(10); // Kings can move to at most 8 regular squares + 2 castling moves
    
    const int x = pos.first;
    const int y = pos.second;
    
    // Fast check to avoid unnecessary calculations
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return moves;
    
    // Get the opponent's color for attack checking
    const PieceColor opponentColor = (piece.color == PieceColor::WHITE) ? 
                                    PieceColor::BLACK : PieceColor::WHITE;
    
    // Define all eight directions using static array for performance
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
                // Check if the square is under attack
                if (!gameLogic.isSquareAttacked(newPos, opponentColor)) {
                    moves.push_back(newPos);
                }
            }
        }
    }
    
    // Check for castling possibilities (only if king is not in check)
    if (!gameLogic.isInCheck()) {
        // Check for castling
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
    
    // Check that the rook is present and is correct type/color
    auto rookIt = pieces.find(rookPos);
    if (rookIt == pieces.end() || 
        rookIt->second.type != PieceType::ROOK || 
        rookIt->second.color != king.color) {
        return false;
    }
    
    // Get opponent color
    const PieceColor opponentColor = (king.color == PieceColor::WHITE) ? 
                                    PieceColor::BLACK : PieceColor::WHITE;
    
    // Check that squares between king and rook are empty
    const int startCol = direction > 0 ? x + 1 : rookPos.first + 1;
    const int endCol = direction > 0 ? rookPos.first - 1 : x - 1;
    
    for (int col = std::min(startCol, endCol); col <= std::max(startCol, endCol); col++) {
        BoardPosition betweenPos{col, y};
        if (pieces.find(betweenPos) != pieces.end()) {
            return false; // There's a piece in between
        }
    }
    
    // Check that king doesn't move through check
    for (int col = x; col != x + 2 * direction; col += direction) {
        BoardPosition throughPos{col, y};
        if (gameLogic.isSquareAttacked(throughPos, opponentColor)) {
            return false; // King would move through check
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
    // Draw selection highlight
    if (selectedSquare.first != -1) {
        sf::RectangleShape highlight(sf::Vector2f(squareSize, squareSize));
        highlight.setPosition(selectedSquare.first * squareSize, selectedSquare.second * squareSize);
        highlight.setFillColor(currentSelectionColor);
        window.draw(highlight);
    }
    
    // Draw legal move highlights
    for (const auto& move : legalMoves) {
        sf::RectangleShape moveHighlight(sf::Vector2f(squareSize, squareSize));
        moveHighlight.setPosition(move.first * squareSize, move.second * squareSize);
        moveHighlight.setFillColor(legalMoveColor);
        window.draw(moveHighlight);
    }
    
    // Draw check highlight if king is in check
    if (gameLogic.getGameState() == GameState::CHECK) {
        // Find the king position
        BoardPosition kingPos = {-1, -1};
                               
        // Look for the king of the current player
        for (const auto& [pos, piece] : pieces) {
            if (piece.type == PieceType::KING && piece.color == gameLogic.getCurrentTurn()) {
                kingPos = pos;
                break;
            }
        }
        
        // Highlight the king in check
        if (kingPos.first != -1) {
            sf::RectangleShape checkHighlight(sf::Vector2f(squareSize, squareSize));
            checkHighlight.setPosition(kingPos.first * squareSize, kingPos.second * squareSize);
            checkHighlight.setFillColor(checkHighlightColor);
            window.draw(checkHighlight);
        }
    }
    
    // Draw promotion UI if active
    if (awaitingPromotion) {
        // Draw the border
        window.draw(promotionBorder);
        
        // Draw the panel
        window.draw(promotionPanel);
        
        // Draw the header
        window.draw(promotionHeader);
        
        // Draw selection highlights and pieces
        for (size_t i = 0; i < promotionOptions.size(); ++i) {
            window.draw(promotionSelectionHighlights[i]);
            window.draw(promotionOptions[i].sprite);
        }
    }
}