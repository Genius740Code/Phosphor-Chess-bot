#include "pieces_movment.h"
#include <iostream>

ChessInteraction::ChessInteraction(std::map<BoardPosition, ChessPiece>& piecesRef, float squareSz)
    : pieces(piecesRef), squareSize(squareSz)
{
    // Initialize promotion panel
    const float panelWidth = squareSize * 1.5f;
    const float headerHeight = squareSize * 0.4f;
    const float panelHeight = 4 * squareSize + headerHeight;
    const float borderThickness = 3.0f;
    
    // Main panel
    promotionPanel.setSize(sf::Vector2f(panelWidth, panelHeight));
    promotionPanel.setFillColor(promotionPanelColor);
    
    // Header
    promotionHeader.setSize(sf::Vector2f(panelWidth, headerHeight));
    promotionHeader.setFillColor(promotionHeaderColor);
    
    // Border
    promotionBorder.setSize(sf::Vector2f(panelWidth + 2*borderThickness, panelHeight + 2*borderThickness));
    promotionBorder.setFillColor(promotionBorderColor);
    
    // Create highlight boxes for selection feedback
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape highlight;
        highlight.setSize(sf::Vector2f(panelWidth - 10, squareSize - 10));
        highlight.setFillColor(sf::Color(173, 216, 230, 120));
        highlight.setOutlineThickness(2);
        highlight.setOutlineColor(sf::Color(100, 149, 237));
        promotionSelectionHighlights.push_back(highlight);
    }
}

bool ChessInteraction::handleMouseClick(int x, int y) {
    // Keep track of old selection to determine if state changed
    auto oldSelection = selectedSquare;
    
    // Check if we're handling a promotion selection
    if (awaitingPromotion) {
        // Get promotion panel bounds
        float panelX = promotionPanel.getPosition().x;
        float panelY = promotionPanel.getPosition().y;
        float panelWidth = promotionPanel.getSize().x;
        float panelHeight = promotionPanel.getSize().y;
        float headerHeight = promotionHeader.getSize().y;
        
        // Check if click is within promotion panel
        if (x >= panelX && x <= panelX + panelWidth &&
            y >= panelY + headerHeight && y <= panelY + panelHeight) {
            
            // Calculate which option was clicked
            int relativeY = y - (panelY + headerHeight);
            int optionIndex = relativeY / static_cast<int>(squareSize);
            
            // Make sure it's a valid selection
            if (optionIndex >= 0 && optionIndex < 4) { // Queen, Rook, Bishop, Knight
                PieceType promotionType;
                switch (optionIndex) {
                    case 0: promotionType = PieceType::QUEEN; break;
                    case 1: promotionType = PieceType::ROOK; break;
                    case 2: promotionType = PieceType::BISHOP; break;
                    case 3: promotionType = PieceType::KNIGHT; break;
                    default: promotionType = PieceType::QUEEN; break;
                }
                
                executePromotion(promotionType);
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
    int file = x / static_cast<int>(squareSize);
    int rank = y / static_cast<int>(squareSize);
    
    // Ensure we're within board boundaries
    if (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
        BoardPosition clickedSquare = {file, rank};
        
        // Check if we clicked on a legal move square
        if (selectedSquare.first != -1) {
            auto it = std::find(legalMoves.begin(), legalMoves.end(), clickedSquare);
            if (it != legalMoves.end()) {
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
    
    // Check for promotion
    if (isPromotionMove(from, to)) {
        // Save the move info and show promotion options
        showPromotionOptions(to, movingPiece.color);
        
        // Center the pawn in the destination square properly
        const sf::Texture* texture = movingPiece.sprite.getTexture();
        if (texture) {
            float textureWidth = texture->getSize().x;
            float textureHeight = texture->getSize().y;
            float scaleX = movingPiece.sprite.getScale().x;
            float scaleY = movingPiece.sprite.getScale().y;
            
            // Calculate position to center the piece in the square
            float offsetX = (squareSize - (textureWidth * scaleX)) / 2;
            float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
            
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
        pieces[to] = movingPiece;
        
        // Set flag to await promotion choice
        awaitingPromotion = true;
        promotionSquare = to;
        promotionColor = movingPiece.color;
        
        return;
    }
    
    // Check for en passant capture
    if (isEnPassantMove(from, to)) {
        // Remove the captured pawn (which is not at the destination square)
        BoardPosition capturedPawnPos = {to.first, from.second};
        pieces.erase(capturedPawnPos);
        std::cout << "En passant capture at (" << capturedPawnPos.first << "," 
                  << capturedPawnPos.second << ")" << std::endl;
    }
    
    // Capture any piece at destination
    pieces.erase(to);
    
    // Center the piece properly in its destination square
    const sf::Texture* texture = movingPiece.sprite.getTexture();
    if (texture) {
        float textureWidth = texture->getSize().x;
        float textureHeight = texture->getSize().y;
        float scaleX = movingPiece.sprite.getScale().x;
        float scaleY = movingPiece.sprite.getScale().y;
        
        // Calculate position to center the piece in the square
        float offsetX = (squareSize - (textureWidth * scaleX)) / 2;
        float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
        
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
    pieces[to] = movingPiece;
    
    // Check if this is a two-square pawn move (for en passant next move)
    if (movingPiece.type == PieceType::PAWN && std::abs(to.second - from.second) == 2) {
        // Set en passant target square
        enPassantTarget = {to.first, (from.second + to.second) / 2}; // Middle square
        std::cout << "En passant target set at (" << enPassantTarget.first << "," 
                  << enPassantTarget.second << ")" << std::endl;
    } else {
        // Clear en passant target
        enPassantTarget = {-1, -1};
    }
    
    std::cout << "Piece moved from (" << from.first << "," 
              << from.second << ") to (" 
              << to.first << "," << to.second << ")" << std::endl;
}

bool ChessInteraction::isPromotionMove(const BoardPosition& from, const BoardPosition& to) const {
    auto pieceIt = pieces.find(from);
    if (pieceIt == pieces.end()) return false;
    
    const ChessPiece& piece = pieceIt->second;
    
    // Check if it's a pawn
    if (piece.type != PieceType::PAWN) return false;
    
    // Check if it's reaching the opposite end
    if ((piece.color == PieceColor::WHITE && to.second == 0) ||
        (piece.color == PieceColor::BLACK && to.second == 7)) {
        return true;
    }
    
    return false;
}

bool ChessInteraction::isEnPassantMove(const BoardPosition& from, const BoardPosition& to) const {
    // If there's no en passant target, return false
    if (enPassantTarget.first == -1) return false;
    
    auto pieceIt = pieces.find(from);
    if (pieceIt == pieces.end()) return false;
    
    const ChessPiece& piece = pieceIt->second;
    
    // Check if it's a pawn
    if (piece.type != PieceType::PAWN) return false;
    
    // Check if destination is the en passant target
    if (to == enPassantTarget) {
        // Check if it's a diagonal move (capture)
        if (std::abs(to.first - from.first) == 1 && std::abs(to.second - from.second) == 1) {
            return true;
        }
    }
    
    return false;
}

void ChessInteraction::showPromotionOptions(const BoardPosition& square, PieceColor color) {
    promotionOptions.clear();
    
    // Define the pieces to choose from (in order: Queen, Rook, Bishop, Knight)
    PieceType promotionTypes[] = {
        PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT
    };
    
    // Calculate optimal position for the promotion panel
    const float panelWidth = promotionPanel.getSize().x;
    const float panelHeight = promotionPanel.getSize().y;
    const float headerHeight = promotionHeader.getSize().y;
    const float borderThickness = 3.0f;
    
    // Position the panel to the right of the column when possible, otherwise to the left
    float panelX;
    if (square.first < 6) {  // If there's room to the right
        panelX = (square.first + 1.2f) * squareSize;
    } else {  // Position to the left
        panelX = (square.first - 1.7f) * squareSize;
    }
    
    // Position vertically to be centered
    float panelY = std::max(0.0f, std::min(
        (8.0f * squareSize) - panelHeight,  // Don't go below bottom
        square.second * squareSize - (panelHeight / 2) + (squareSize / 2)  // Center on square
    ));
    
    // Set border position (slightly offset to create border effect)
    promotionBorder.setPosition(panelX - borderThickness, panelY - borderThickness);
    
    // Set panel position
    promotionPanel.setPosition(panelX, panelY);
    
    // Set header position
    promotionHeader.setPosition(panelX, panelY);
    
    // Get texture manager reference
    auto& textureManager = PieceTextureManager::getInstance();
    
    // Create sprite for each promotion option
    for (int i = 0; i < 4; i++) {
        // Construct the texture key
        std::string colorName = (color == PieceColor::WHITE) ? "white" : "black";
        std::string typeName;
        switch (promotionTypes[i]) {
            case PieceType::QUEEN: typeName = "queen"; break;
            case PieceType::ROOK: typeName = "rook"; break;
            case PieceType::BISHOP: typeName = "bishop"; break;
            case PieceType::KNIGHT: typeName = "knight"; break;
            default: continue;
        }
        
        std::string textureKey = colorName + "-" + typeName;
        const sf::Texture* texture = textureManager.getTexture(textureKey);
        
        if (texture) {
            ChessPiece option{promotionTypes[i], color, sf::Sprite(*texture)};
            
            // Scale the sprite
            float textureWidth = texture->getSize().x;
            float textureHeight = texture->getSize().y;
            float scaleFactor = textureManager.getScale() * 0.9f; // Slightly smaller
            float scaleX = (squareSize / textureWidth) * scaleFactor;
            float scaleY = (squareSize / textureHeight) * scaleFactor;
            option.sprite.setScale(scaleX, scaleY);
            
            // Center the piece in its panel slot
            float offsetX = (panelWidth - (textureWidth * scaleX)) / 2;
            float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
            
            float optionY = panelY + headerHeight + (i * squareSize);
            option.sprite.setPosition(panelX + offsetX, optionY + offsetY);
            
            // Position the selection highlight
            promotionSelectionHighlights[i].setPosition(
                panelX + 5, 
                optionY + 5
            );
            
            promotionOptions.push_back(option);
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
    
    if (!texture) {
        std::cerr << "Error: Could not find texture for " << textureKey << std::endl;
        return;
    }
    
    // Create the promoted piece
    ChessPiece promotedPiece{
        promotionType,
        promotionColor,
        sf::Sprite(*texture)
    };
    
    // Set appropriate scale and position
    float textureWidth = texture->getSize().x;
    float textureHeight = texture->getSize().y;
    float scaleFactor = PieceTextureManager::getInstance().getScale();
    float scaleX = (squareSize / textureWidth) * scaleFactor;
    float scaleY = (squareSize / textureHeight) * scaleFactor;
    promotedPiece.sprite.setScale(scaleX, scaleY);
    
    // Center the piece in its square
    float offsetX = (squareSize - (textureWidth * scaleX)) / 2;
    float offsetY = (squareSize - (textureHeight * scaleY)) / 2;
    
    promotedPiece.sprite.setPosition(
        promotionSquare.first * squareSize + offsetX,
        promotionSquare.second * squareSize + offsetY
    );
    
    // Replace the pawn with the promoted piece
    pieces[promotionSquare] = promotedPiece;
    
    std::cout << "Promoted pawn to " << typeName << " at (" 
              << promotionSquare.first << "," << promotionSquare.second << ")" << std::endl;
}

void ChessInteraction::calculateLegalMoves() {
    // Clear previous legal moves
    legalMoves.clear();
    
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
            // King moves will be implemented later
            break;
    }
}

std::vector<BoardPosition> ChessInteraction::getPawnMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    int x = pos.first;
    int y = pos.second;
    
    // Determine direction (white pawns move up the board, black pawns down)
    int direction = (piece.color == PieceColor::WHITE) ? -1 : 1;
    
    // Forward move (can't capture)
    BoardPosition forwardPos{x, y + direction};
    if (forwardPos.second >= 0 && forwardPos.second < 8) {
        // Check if the square is empty
        if (pieces.find(forwardPos) == pieces.end()) {
            moves.push_back(forwardPos);
            
            // If pawn is in starting position, it can move two squares forward
            bool inStartingPos = (piece.color == PieceColor::WHITE && y == 6) || 
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
    
    // Diagonal captures
    for (int dx : {-1, 1}) {
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
    
    // Rooks move horizontally and vertically
    // Check four directions: up, right, down, left
    addMovesInDirection(moves, pos, 0, -1, piece); // Up
    addMovesInDirection(moves, pos, 1, 0, piece);  // Right
    addMovesInDirection(moves, pos, 0, 1, piece);  // Down
    addMovesInDirection(moves, pos, -1, 0, piece); // Left
    
    return moves;
}

std::vector<BoardPosition> ChessInteraction::getKnightMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    int x = pos.first;
    int y = pos.second;
    
    // All eight possible knight moves
    const std::vector<std::pair<int, int>> knightOffsets = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (const auto& offset : knightOffsets) {
        int newX = x + offset.first;
        int newY = y + offset.second;
        
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
    
    // Bishops move diagonally
    // Check four diagonal directions
    addMovesInDirection(moves, pos, 1, -1, piece);  // Up-right
    addMovesInDirection(moves, pos, 1, 1, piece);   // Down-right
    addMovesInDirection(moves, pos, -1, 1, piece);  // Down-left
    addMovesInDirection(moves, pos, -1, -1, piece); // Up-left
    
    return moves;
}

std::vector<BoardPosition> ChessInteraction::getQueenMoves(const BoardPosition& pos, const ChessPiece& piece) {
    std::vector<BoardPosition> moves;
    
    // Queens can move like both rooks and bishops
    // First get all rook moves
    auto rookMoves = getRookMoves(pos, piece);
    moves.insert(moves.end(), rookMoves.begin(), rookMoves.end());
    
    // Then get all bishop moves
    auto bishopMoves = getBishopMoves(pos, piece);
    moves.insert(moves.end(), bishopMoves.begin(), bishopMoves.end());
    
    return moves;
}

void ChessInteraction::addMovesInDirection(std::vector<BoardPosition>& moves, 
                                          const BoardPosition& startPos, 
                                          int dirX, int dirY, 
                                          const ChessPiece& piece) {
    int x = startPos.first;
    int y = startPos.second;
    
    for (int i = 1; i < 8; ++i) {
        int newX = x + i * dirX;
        int newY = y + i * dirY;
        
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
        for (int i = 0; i < 4; i++) {
            promotionSelectionHighlights[i].setFillColor(sf::Color(173, 216, 230, 60));
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
    
    // Draw promotion UI if active - we'll draw this after everything else
    // to ensure it's on top, but we'll save it for later
    
    // ... any other UI elements would be drawn here
    
    // Now draw the promotion UI if active - this ensures it appears on top of everything
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
        for (int i = 0; i < std::min(static_cast<int>(promotionOptions.size()), 4); i++) {
            window.draw(promotionSelectionHighlights[i]);
        }
        
        // Draw promotion piece options
        for (const auto& option : promotionOptions) {
            window.draw(option.sprite);
        }
    }
}

// The rest of the ChessInteraction implementation remains unchanged...