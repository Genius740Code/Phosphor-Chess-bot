#include "game_logic.h"
#include <algorithm>
#include <sstream>
#include <iostream>

// Constructor
ChessGameLogic::ChessGameLogic(std::map<BoardPosition, ChessPiece>& piecesRef)
    : pieces(piecesRef),
      currentTurn(PieceColor::WHITE),
      gameState(GameState::ACTIVE),
      halfMoveClock(0),
      fullMoveCounter(1),
      enPassantAvailable(false),
      whiteKingPosition(-1, -1),
      blackKingPosition(-1, -1)
{
    // Find the kings
    updateKingPositions();
    
    // Store initial position
    positionHistory.push_back(getCurrentPositionFEN());
}

// Find and update king positions
void ChessGameLogic::updateKingPositions() {
    for (const auto& [pos, piece] : pieces) {
        if (piece.type == PieceType::KING) {
            if (piece.color == PieceColor::WHITE) {
                whiteKingPosition = pos;
            } else {
                blackKingPosition = pos;
            }
        }
    }
}

// Calculate a hash of the current board state for cache
std::size_t ChessGameLogic::calculateBoardHash() const {
    std::size_t hash = 0;
    
    // Hash each piece and its position on the board
    for (const auto& [pos, piece] : pieces) {
        // Combine position, piece type, and color
        std::size_t pieceHash = BoardPositionHash()(pos) ^
                               (static_cast<std::size_t>(piece.type) << 3) ^
                               (static_cast<std::size_t>(piece.color) << 6);
                               
        // Mix into the overall hash
        hash ^= pieceHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Also include en passant target in the hash
    if (enPassantAvailable) {
        hash ^= BoardPositionHash()(enPassantTarget) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    return hash;
}

// Check if a square is under attack by a piece of the specified color
bool ChessGameLogic::isSquareAttacked(const BoardPosition& square, PieceColor attackingColor) const {
    // Check the cache first - this is a hot path in the chess engine
    const std::size_t boardHash = calculateBoardHash();
    const AttackedSquareKey key{square, attackingColor, boardHash};
    
    auto it = attackedSquareCache.find(key);
    if (it != attackedSquareCache.end()) {
        return it->second;
    }
    
    const int targetX = square.first;
    const int targetY = square.second;
    
    // Use local variables for better cache locality
    bool isAttacked = false;
    
    // Check for pawn attacks - most common and simple check, do this first
    const int pawnDirection = (attackingColor == PieceColor::WHITE) ? 1 : -1;
    for (int dx : {-1, 1}) {
        const int pawnX = targetX + dx;
        const int pawnY = targetY + pawnDirection;
        
        // Check board bounds with a single condition
        if (pawnX >= 0 && pawnX < 8 && pawnY >= 0 && pawnY < 8) {
            const BoardPosition pawnPos(pawnX, pawnY);
            auto pieceIt = pieces.find(pawnPos);
            if (pieceIt != pieces.end() && 
                pieceIt->second.color == attackingColor && 
                pieceIt->second.type == PieceType::PAWN) {
                isAttacked = true;
                break;
            }
        }
    }
    
    if (isAttacked) {
        attackedSquareCache[key] = true;
        return true;
    }
    
    // Check for knight attacks - knights can jump over pieces, so this is also simple
    // Pre-compute all 8 knight moves for efficiency
    static const std::pair<int, int> knightMoves[] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (const auto& [dx, dy] : knightMoves) {
        const int knightX = targetX + dx;
        const int knightY = targetY + dy;
        
        // Bounds check
        if (knightX >= 0 && knightX < 8 && knightY >= 0 && knightY < 8) {
            const BoardPosition knightPos(knightX, knightY);
            auto pieceIt = pieces.find(knightPos);
            if (pieceIt != pieces.end() && 
                pieceIt->second.color == attackingColor && 
                pieceIt->second.type == PieceType::KNIGHT) {
                isAttacked = true;
                break;
            }
        }
    }
    
    if (isAttacked) {
        attackedSquareCache[key] = true;
        return true;
    }
    
    // Check king attacks (for adjacent squares)
    // Pre-compute all 8 king moves for efficiency
    static const std::pair<int, int> kingMoves[] = {
        {-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
        {0, 1}, {1, -1}, {1, 0}, {1, 1}
    };
    
    for (const auto& [dx, dy] : kingMoves) {
        const int kingX = targetX + dx;
        const int kingY = targetY + dy;
        
        // Bounds check
        if (kingX >= 0 && kingX < 8 && kingY >= 0 && kingY < 8) {
            const BoardPosition kingPos(kingX, kingY);
            auto pieceIt = pieces.find(kingPos);
            if (pieceIt != pieces.end() && 
                pieceIt->second.color == attackingColor && 
                pieceIt->second.type == PieceType::KING) {
                isAttacked = true;
                break;
            }
        }
    }
    
    if (isAttacked) {
        attackedSquareCache[key] = true;
        return true;
    }
    
    // Check for sliding pieces (rook, bishop, queen) attacks
    // Pre-compile all directions for sliding pieces
    static const std::pair<int, int> rookDirections[] = {
        {0, 1}, {1, 0}, {0, -1}, {-1, 0}  // Vertical and horizontal
    };
    
    static const std::pair<int, int> bishopDirections[] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}  // Diagonals
    };
    
    // Check rook-like moves (rook and queen)
    for (const auto& [dx, dy] : rookDirections) {
        int x = targetX + dx;
        int y = targetY + dy;
        
        while (x >= 0 && x < 8 && y >= 0 && y < 8) {
            const BoardPosition pos(x, y);
            auto pieceIt = pieces.find(pos);
            
            if (pieceIt != pieces.end()) {
                // Found a piece
                if (pieceIt->second.color == attackingColor &&
                    (pieceIt->second.type == PieceType::ROOK || 
                     pieceIt->second.type == PieceType::QUEEN)) {
                    isAttacked = true;
                    break;
                }
                // Any other piece blocks this direction
                break;
            }
            
            // Continue in the same direction
            x += dx;
            y += dy;
        }
        
        if (isAttacked) break;
    }
    
    if (isAttacked) {
        attackedSquareCache[key] = true;
        return true;
    }
    
    // Check bishop-like moves (bishop and queen)
    for (const auto& [dx, dy] : bishopDirections) {
        int x = targetX + dx;
        int y = targetY + dy;
        
        while (x >= 0 && x < 8 && y >= 0 && y < 8) {
            const BoardPosition pos(x, y);
            auto pieceIt = pieces.find(pos);
            
            if (pieceIt != pieces.end()) {
                // Found a piece
                if (pieceIt->second.color == attackingColor &&
                    (pieceIt->second.type == PieceType::BISHOP || 
                     pieceIt->second.type == PieceType::QUEEN)) {
                    isAttacked = true;
                    break;
                }
                // Any other piece blocks this direction
                break;
            }
            
            // Continue in the same direction
            x += dx;
            y += dy;
        }
        
        if (isAttacked) break;
    }
    
    // Store result in cache
    attackedSquareCache[key] = isAttacked;
    return isAttacked;
}

// Switch turns
void ChessGameLogic::switchTurn() {
    // Clear attack cache when turn switches
    clearCache();
    
    // Switch the current player
    currentTurn = (currentTurn == PieceColor::WHITE) ? PieceColor::BLACK : PieceColor::WHITE;
    
    // If it's white's turn again, increment the full move counter
    if (currentTurn == PieceColor::WHITE) {
        fullMoveCounter++;
    }
    
    // First, check if the current player is in check
    bool inCheck = isInCheck();
    
    // Then check for the most common states in order of likelihood
    if (inCheck) {
        // Check for checkmate (no legal moves while in check)
        if (!hasLegalMoves(currentTurn)) {
            gameState = GameState::CHECKMATE;
            return;
        }
        gameState = GameState::CHECK;
        return;
    }
    
    // Check for stalemate (no legal moves but not in check)
    if (!hasLegalMoves(currentTurn)) {
        gameState = GameState::STALEMATE;
        return;
    }
    
    // Check for draw conditions in order of computational expense
    if (halfMoveClock >= 100) { // 50 moves = 100 half-moves
        gameState = GameState::DRAW_FIFTY;
        return;
    }
    
    // Only check for material insufficiency if there are few pieces
    if (pieces.size() <= 4) { // King + King + potentially two more pieces
        if (hasInsufficientMaterial()) {
            gameState = GameState::DRAW_MATERIAL;
            return;
        }
    }
    
    // The most expensive check, only do if position history is long enough
    if (positionHistory.size() >= 9) { // At least 5 full moves made
        if (isDrawByRepetition()) {
            gameState = GameState::DRAW_REPETITION;
            return;
        }
    }
    
    // Otherwise, game is active
    gameState = GameState::ACTIVE;
}

// Check if the king of the specified color is in check
bool ChessGameLogic::isKingInCheck(PieceColor kingColor) const {
    const BoardPosition& kingPos = (kingColor == PieceColor::WHITE) ? 
                                  whiteKingPosition : blackKingPosition;
    
    // If king position is invalid, return false
    if (kingPos.first == -1) {
        return false;
    }
    
    // Opposing color
    PieceColor opponentColor = (kingColor == PieceColor::WHITE) ? 
                              PieceColor::BLACK : PieceColor::WHITE;
    
    return isSquareAttacked(kingPos, opponentColor);
}

// Check if the current player is in check
bool ChessGameLogic::isInCheck() const {
    return isKingInCheck(currentTurn);
}

// Check if the current position is checkmate
bool ChessGameLogic::isCheckmate() const {
    // If not in check, can't be checkmate
    if (!isInCheck()) {
        return false;
    }
    
    // Check if any legal move exists
    return !hasLegalMoves(currentTurn);
}

// Check if the current position is stalemate
bool ChessGameLogic::isStalemate() const {
    // If in check, can't be stalemate
    if (isInCheck()) {
        return false;
    }
    
    // Check if any legal move exists
    return !hasLegalMoves(currentTurn);
}

// Check if any legal move exists for the specified color
bool ChessGameLogic::hasLegalMoves(PieceColor playerColor) const {
    // Iterate through all pieces of the current player
    for (const auto& [pos, piece] : pieces) {
        if (piece.color == playerColor) {
            // Check basic pawn moves
            if (piece.type == PieceType::PAWN) {
                // Forward move
                int direction = (playerColor == PieceColor::WHITE) ? -1 : 1;
                BoardPosition forward(pos.first, pos.second + direction);
                
                // Check if the forward square is valid and empty
                if (forward.second >= 0 && forward.second < 8) {
                    if (pieces.find(forward) == pieces.end()) {
                        // Check if this move would leave the king in check
                        auto tempPieces = pieces;
                        tempPieces[forward] = tempPieces[pos];
                        tempPieces.erase(pos);
                        
                        // Find king position
                        BoardPosition kingPos = (playerColor == PieceColor::WHITE) ? 
                                                whiteKingPosition : blackKingPosition;
                                                
                        // Check if king is in check after move
                        PieceColor opponentColor = (playerColor == PieceColor::WHITE) ? 
                                                  PieceColor::BLACK : PieceColor::WHITE;
                                                  
                        if (!isSquareAttacked(kingPos, opponentColor)) {
                            return true;  // Found a legal move
                        }
                    }
                }
                
                // Check diagonal captures
                for (int dx : {-1, 1}) {
                    BoardPosition capture(pos.first + dx, pos.second + direction);
                    if (capture.first >= 0 && capture.first < 8 && 
                        capture.second >= 0 && capture.second < 8) {
                        
                        auto it = pieces.find(capture);
                        if (it != pieces.end() && it->second.color != playerColor) {
                            // Check if this move would leave the king in check
                            auto tempPieces = pieces;
                            tempPieces[capture] = tempPieces[pos];
                            tempPieces.erase(pos);
                            
                            // Find king position
                            BoardPosition kingPos = (playerColor == PieceColor::WHITE) ? 
                                                   whiteKingPosition : blackKingPosition;
                                                   
                            // Check if king is in check after move
                            PieceColor opponentColor = (playerColor == PieceColor::WHITE) ? 
                                                     PieceColor::BLACK : PieceColor::WHITE;
                                                     
                            if (!isSquareAttacked(kingPos, opponentColor)) {
                                return true;  // Found a legal move
                            }
                        }
                    }
                }
            }
            
            // Check knight moves
            if (piece.type == PieceType::KNIGHT) {
                static const std::pair<int, int> knightMoves[8] = {
                    {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                    {1, -2}, {1, 2}, {2, -1}, {2, 1}
                };
                
                for (const auto& [dx, dy] : knightMoves) {
                    BoardPosition target(pos.first + dx, pos.second + dy);
                    
                    // Check if the target is on the board
                    if (target.first >= 0 && target.first < 8 && 
                        target.second >= 0 && target.second < 8) {
                        
                        // Check if the target square is empty or has an opponent's piece
                        auto it = pieces.find(target);
                        if (it == pieces.end() || it->second.color != playerColor) {
                            // Check if this move would leave the king in check
                            auto tempPieces = pieces;
                            tempPieces[target] = tempPieces[pos];
                            tempPieces.erase(pos);
                            
                            // Find king position
                            BoardPosition kingPos = (playerColor == PieceColor::WHITE) ? 
                                                   whiteKingPosition : blackKingPosition;
                                                   
                            // Check if king is in check after move
                            PieceColor opponentColor = (playerColor == PieceColor::WHITE) ? 
                                                      PieceColor::BLACK : PieceColor::WHITE;
                                                      
                            if (!isSquareAttacked(kingPos, opponentColor)) {
                                return true;  // Found a legal move
                            }
                        }
                    }
                }
            }
            
            // For other pieces, use the existing getLegalMoves method
            // Get all potential moves for this piece
            std::vector<BoardPosition> moves = getLegalMoves(pos);
            
            // If any legal move exists, return true
            if (!moves.empty()) {
                return true;
            }
        }
    }
    
    // No legal moves found
    return false;
}

// Helper method to check if a king would be in check after a move
bool ChessGameLogic::wouldBeInCheck(const BoardPosition& kingPos, PieceColor kingColor, 
                                    const std::map<BoardPosition, ChessPiece>& boardState) const {
    PieceColor opponentColor = (kingColor == PieceColor::WHITE) ? PieceColor::BLACK : PieceColor::WHITE;
    return isSquareAttackedByPieces(kingPos, opponentColor, boardState);
}

// Check if a square is under attack by a piece of the specified color in a given board state
bool ChessGameLogic::isSquareAttackedByPieces(const BoardPosition& square, PieceColor attackingColor,
                                             const std::map<BoardPosition, ChessPiece>& boardState) const {
    const int targetX = square.first;
    const int targetY = square.second;
    
    // Check for pawn attacks - most common and simple check, do this first
    const int pawnDirection = (attackingColor == PieceColor::WHITE) ? 1 : -1;
    for (int dx : {-1, 1}) {
        const int pawnX = targetX + dx;
        const int pawnY = targetY + pawnDirection;
        
        // Check board bounds
        if (pawnX >= 0 && pawnX < 8 && pawnY >= 0 && pawnY < 8) {
            BoardPosition pawnPos(pawnX, pawnY);
            auto it = boardState.find(pawnPos);
            if (it != boardState.end() && it->second.color == attackingColor && 
                it->second.type == PieceType::PAWN) {
                return true;
            }
        }
    }
    
    // Check for knight attacks - knights can jump over pieces, so this is also simple
    static const std::pair<int, int> knightMoves[8] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (const auto& [dx, dy] : knightMoves) {
        const int knightX = targetX + dx;
        const int knightY = targetY + dy;
        
        // Check board bounds
        if (knightX >= 0 && knightX < 8 && knightY >= 0 && knightY < 8) {
            BoardPosition knightPos(knightX, knightY);
            auto it = boardState.find(knightPos);
            if (it != boardState.end() && it->second.color == attackingColor && 
                it->second.type == PieceType::KNIGHT) {
                return true;
            }
        }
    }
    
    // Check king attacks (for adjacent squares)
    static const std::pair<int, int> kingMoves[8] = {
        {-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
        {0, 1}, {1, -1}, {1, 0}, {1, 1}
    };
    
    for (const auto& [dx, dy] : kingMoves) {
        const int kingX = targetX + dx;
        const int kingY = targetY + dy;
        
        // Check board bounds
        if (kingX >= 0 && kingX < 8 && kingY >= 0 && kingY < 8) {
            BoardPosition kingPos(kingX, kingY);
            auto it = boardState.find(kingPos);
            if (it != boardState.end() && it->second.color == attackingColor && 
                it->second.type == PieceType::KING) {
                return true;
            }
        }
    }
    
    // Check for sliding pieces (rook, bishop, queen) attacks
    // Pre-compile all directions for sliding pieces
    static const std::pair<int, int> slidingDirections[8] = {
        {0, 1}, {1, 0}, {0, -1}, {-1, 0},  // Rook directions
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}  // Bishop directions
    };
    
    for (int i = 0; i < 8; i++) {
        const auto& [dx, dy] = slidingDirections[i];
        bool isRookDirection = i < 4;  // First 4 are rook directions
        
        int x = targetX + dx;
        int y = targetY + dy;
        
        while (x >= 0 && x < 8 && y >= 0 && y < 8) {
            BoardPosition pos(x, y);
            auto it = boardState.find(pos);
            
            if (it != boardState.end()) {
                // Found a piece
                if (it->second.color == attackingColor) {
                    if ((isRookDirection && (it->second.type == PieceType::ROOK || it->second.type == PieceType::QUEEN)) ||
                        (!isRookDirection && (it->second.type == PieceType::BISHOP || it->second.type == PieceType::QUEEN))) {
                        return true;
                    }
                }
                // Blocking piece, stop checking in this direction
                break;
            }
            
            // Continue in the same direction
            x += dx;
            y += dy;
        }
    }
    
    // No attacks found
    return false;
}

// Check if the position has insufficient material for checkmate
bool ChessGameLogic::hasInsufficientMaterial() const {
    // Count pieces by type and color
    int whiteBishops = 0, whiteKnights = 0;
    int blackBishops = 0, blackKnights = 0;
    
    // First check for pawns, rooks and queens - if any exist, it's not insufficient material
    for (const auto& [pos, piece] : pieces) {
        switch (piece.type) {
            case PieceType::PAWN:
                return false; // Pawns can promote, so it's not a draw
            case PieceType::ROOK:
            case PieceType::QUEEN:
                return false; // Rooks and queens can force mate
            case PieceType::BISHOP:
                if (piece.color == PieceColor::WHITE) whiteBishops++;
                else blackBishops++;
                break;
            case PieceType::KNIGHT:
                if (piece.color == PieceColor::WHITE) whiteKnights++;
                else blackKnights++;
                break;
            default: // King - all positions have kings
                break;
        }
    }
    
    // If we have no pawns, rooks, or queens, then check knight and bishop combinations
    
    // King vs. King
    if (whiteBishops == 0 && whiteKnights == 0 && blackBishops == 0 && blackKnights == 0) {
        return true;
    }
    
    // King + Knight vs King
    if ((whiteBishops == 0 && whiteKnights == 1 && blackBishops == 0 && blackKnights == 0) ||
        (whiteBishops == 0 && whiteKnights == 0 && blackBishops == 0 && blackKnights == 1)) {
        return true;
    }
    
    // King + Bishop vs King
    if ((whiteBishops == 1 && whiteKnights == 0 && blackBishops == 0 && blackKnights == 0) ||
        (whiteBishops == 0 && whiteKnights == 0 && blackBishops == 1 && blackKnights == 0)) {
        return true;
    }
    
    // King + Bishop vs King + Bishop (same color bishops)
    if (whiteBishops == 1 && whiteKnights == 0 && blackBishops == 1 && blackKnights == 0) {
        // Check if bishops move on same colored squares
        bool whiteBishopOnLight = false;
        bool blackBishopOnLight = false;
        
        for (const auto& [pos, piece] : pieces) {
            if (piece.type == PieceType::BISHOP) {
                // A square is light if the sum of its coordinates is even
                bool onLightSquare = ((pos.first + pos.second) % 2 == 0);
                
                if (piece.color == PieceColor::WHITE) {
                    whiteBishopOnLight = onLightSquare;
                } else {
                    blackBishopOnLight = onLightSquare;
                }
            }
        }
        
        // If both bishops are on same colored squares, it's a draw
        if (whiteBishopOnLight == blackBishopOnLight) {
            return true;
        }
    }
    
    return false;
}

// Check if the position qualifies for a draw
bool ChessGameLogic::isDraw() const {
    return isStalemate() || isDraw50MoveRule() || isDrawByRepetition() || isDrawByInsufficientMaterial();
}

// Check if the position is a draw by repetition
bool ChessGameLogic::isDrawByRepetition() const {
    // Threefold repetition check can be expensive, so do a quick count first
    if (positionHistory.size() < 5) {
        return false; // Need at least 5 positions (minimally) for a threefold repetition
    }
    
    // Get current position
    std::string currentPosition = getCurrentPositionFEN();
    
    // Counter for repetitions
    int repetitions = 0;
    
    // Count occurrences of the current position
    for (const auto& position : positionHistory) {
        if (position == currentPosition) {
            repetitions++;
            if (repetitions >= 3) {
                return true; // Found threefold repetition
            }
        }
    }
    
    return false;
}

// Count repetitions of a position in the history
int ChessGameLogic::getRepetitionCount(const std::string& position) const {
    int count = 0;
    for (const auto& pos : positionHistory) {
        if (pos == position) {
            count++;
        }
    }
    return count;
}

// Check if the position is a draw by insufficient material
bool ChessGameLogic::isDrawByInsufficientMaterial() const {
    return hasInsufficientMaterial();
}

// Get current position FEN (Forsyth-Edwards Notation)
std::string ChessGameLogic::getCurrentPositionFEN() const {
    std::ostringstream fen;
    
    // Board position
    for (int rank = 0; rank < 8; ++rank) {
        int emptyCount = 0;
        
        for (int file = 0; file < 8; ++file) {
            BoardPosition pos(file, rank);
            auto it = pieces.find(pos);
            
            if (it != pieces.end()) {
                // If there were empty squares before this piece, add the count
                if (emptyCount > 0) {
                    fen << emptyCount;
                    emptyCount = 0;
                }
                
                // Add the piece character
                char pieceChar = '?';
                switch (it->second.type) {
                    case PieceType::PAWN:   pieceChar = 'p'; break;
                    case PieceType::KNIGHT: pieceChar = 'n'; break;
                    case PieceType::BISHOP: pieceChar = 'b'; break;
                    case PieceType::ROOK:   pieceChar = 'r'; break;
                    case PieceType::QUEEN:  pieceChar = 'q'; break;
                    case PieceType::KING:   pieceChar = 'k'; break;
                }
                
                // Make uppercase for white pieces
                if (it->second.color == PieceColor::WHITE) {
                    pieceChar = std::toupper(pieceChar);
                }
                
                fen << pieceChar;
            } else {
                // Empty square
                emptyCount++;
            }
        }
        
        // If there are empty squares at the end of the rank
        if (emptyCount > 0) {
            fen << emptyCount;
        }
        
        // Add rank separator (except for the last rank)
        if (rank < 7) {
            fen << '/';
        }
    }
    
    // Active color
    fen << ' ' << (currentTurn == PieceColor::WHITE ? 'w' : 'b');
    
    // Since we don't store full castling rights, just add a placeholder
    fen << " - ";
    
    // En passant target square
    if (enPassantAvailable) {
        char file = 'a' + enPassantTarget.first;
        char rank = '8' - enPassantTarget.second;
        fen << ' ' << file << rank;
    } else {
        fen << " - ";
    }
    
    // Halfmove clock and fullmove number
    fen << ' ' << halfMoveClock << ' ' << fullMoveCounter;
    
    return fen.str();
}

// Reset the game to initial position
void ChessGameLogic::resetGame() {
    // Clear cache on reset
    clearCache();
    
    currentTurn = PieceColor::WHITE;
    gameState = GameState::ACTIVE;
    halfMoveClock = 0;
    fullMoveCounter = 1;
    enPassantAvailable = false;
    
    // Clear position history
    positionHistory.clear();
    
    // Set up initial position
    setupFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

// Setup from FEN
void ChessGameLogic::setupFromFEN(const std::string& fen) {
    // Clear cache when setting up new position
    clearCache();
    
    // Clear the existing pieces
    pieces.clear();
    
    // Parse the FEN string
    std::istringstream fenStream(fen);
    std::string positionPart;
    fenStream >> positionPart; // Get the position part of FEN
    
    // Set up pieces based on FEN position string
    int rank = 0;
    int file = 0;
    
    for (char c : positionPart) {
        if (c == '/') {
            // Move to the next rank
            rank++;
            file = 0;
        } else if (std::isdigit(c)) {
            // Skip empty squares
            file += (c - '0');
        } else {
            // Add a piece
            PieceType type;
            PieceColor color = std::islower(c) ? PieceColor::BLACK : PieceColor::WHITE;
            char piece = std::tolower(c);
            
            switch (piece) {
                case 'p': type = PieceType::PAWN; break;
                case 'n': type = PieceType::KNIGHT; break;
                case 'b': type = PieceType::BISHOP; break;
                case 'r': type = PieceType::ROOK; break;
                case 'q': type = PieceType::QUEEN; break;
                case 'k': type = PieceType::KING; break;
                default: continue; // Invalid piece character
            }
            
            // Add the piece to the board
            ChessPiece newPiece;
            newPiece.type = type;
            newPiece.color = color;
            // The sprite will be initialized later when textures are loaded
            pieces[{file, rank}] = newPiece;
            file++;
        }
    }
    
    // Parse the rest of the FEN (active color, castling, etc.)
    // This is a simplified implementation
    std::string activeColor;
    fenStream >> activeColor;
    currentTurn = (activeColor == "w") ? PieceColor::WHITE : PieceColor::BLACK;
    
    // Update king positions after loading
    updateKingPositions();
    
    // Add initial position to history
    positionHistory.push_back(getCurrentPositionFEN());
}

// Offer a draw
void ChessGameLogic::offerDraw(bool accepted) {
    if (accepted) {
        gameState = GameState::DRAW_AGREEMENT;
    }
}

// Check if a move is valid according to chess rules
bool ChessGameLogic::isValidMove(const BoardPosition& from, const BoardPosition& to) const {
    // Check if 'from' position contains a piece
    auto fromIt = pieces.find(from);
    if (fromIt == pieces.end()) {
        return false;
    }
    
    // Check if the piece belongs to the current player
    if (fromIt->second.color != currentTurn) {
        return false;
    }
    
    // Check if 'to' position is valid (either empty or contains opponent's piece)
    auto toIt = pieces.find(to);
    if (toIt != pieces.end() && toIt->second.color == currentTurn) {
        return false; // Cannot capture own piece
    }
    
    const ChessPiece& piece = fromIt->second;
    
    // Check piece-specific movement rules
    bool validPieceMove = false;
    
    switch (piece.type) {
        case PieceType::PAWN: 
            validPieceMove = isValidPawnMove(from, to, piece.color);
            break;
            
        case PieceType::KNIGHT:
            validPieceMove = isValidKnightMove(from, to);
            break;
            
        case PieceType::BISHOP:
            validPieceMove = isValidBishopMove(from, to);
            break;
            
        case PieceType::ROOK:
            validPieceMove = isValidRookMove(from, to);
            break;
            
        case PieceType::QUEEN:
            validPieceMove = isValidQueenMove(from, to);
            break;
            
        case PieceType::KING:
            validPieceMove = isValidKingMove(from, to, piece.color);
            break;
            
        default:
            return false;
    }
    
    if (!validPieceMove) {
        return false;
    }
    
    // Check if the move would leave the king in check
    // Create a temporary board to simulate the move
    auto tempBoard = pieces;
    tempBoard[to] = tempBoard[from];
    tempBoard.erase(from);
    
    // If the piece is a king, update the king position for the check test
    BoardPosition kingPos;
    if (piece.type == PieceType::KING) {
        kingPos = to;
    } else {
        kingPos = (piece.color == PieceColor::WHITE) ? whiteKingPosition : blackKingPosition;
    }
    
    // Check if the king would be in check after the move
    PieceColor opponentColor = (piece.color == PieceColor::WHITE) ? 
                               PieceColor::BLACK : PieceColor::WHITE;
    
    if (isSquareAttackedByPieces(kingPos, opponentColor, tempBoard)) {
        return false; // Move would leave the king in check
    }
    
    return true;
}

// Check if three positions are along the same line (row, column, or diagonal)
bool ChessGameLogic::isAlongLine(const BoardPosition& a, const BoardPosition& b, 
                               const BoardPosition& c) const {
    // Check if the positions are along the same row, column, or diagonal
    
    // Same column
    if (a.first == b.first && b.first == c.first) {
        return true;
    }
    
    // Same row
    if (a.second == b.second && b.second == c.second) {
        return true;
    }
    
    // Check for diagonal alignment
    int dx1 = b.first - a.first;
    int dy1 = b.second - a.second;
    int dx2 = c.first - b.first;
    int dy2 = c.second - b.second;
    
    // If the slopes are the same and non-zero, they're on the same diagonal
    if (dx1 != 0 && dx2 != 0 && dy1 != 0 && dy2 != 0) {
        return (dy1 * dx2 == dy2 * dx1);
    }
    
    return false;
}

// Check if a pawn move is valid
bool ChessGameLogic::isValidPawnMove(const BoardPosition& from, const BoardPosition& to, 
                                   PieceColor color) const {
    const int direction = (color == PieceColor::WHITE) ? -1 : 1;
    const int startingRank = (color == PieceColor::WHITE) ? 6 : 1;
    
    // Calculate move distance
    const int dx = to.first - from.first;
    const int dy = to.second - from.second;
    
    // Forward move (1 square)
    if (dx == 0 && dy == direction) {
        // Check if the target square is empty
        return pieces.find(to) == pieces.end();
    }
    
    // Forward move (2 squares from starting position)
    if (dx == 0 && dy == 2 * direction && from.second == startingRank) {
        // Check if both squares are empty
        BoardPosition middle(from.first, from.second + direction);
        return pieces.find(middle) == pieces.end() && pieces.find(to) == pieces.end();
    }
    
    // Capture move (diagonal)
    if (std::abs(dx) == 1 && dy == direction) {
        // Normal capture - check if the target square has an opponent's piece
        auto it = pieces.find(to);
        if (it != pieces.end() && it->second.color != color) {
            return true;
        }
        
        // En passant capture
        if (enPassantAvailable && to == enPassantTarget) {
            return true;
        }
    }
    
    return false;
}

// Check if a knight move is valid
bool ChessGameLogic::isValidKnightMove(const BoardPosition& from, const BoardPosition& to) const {
    // Knights move in an L-shape: 2 squares in one direction, then 1 square perpendicular
    const int dx = std::abs(to.first - from.first);
    const int dy = std::abs(to.second - from.second);
    
    return (dx == 1 && dy == 2) || (dx == 2 && dy == 1);
}

// Check if a bishop move is valid
bool ChessGameLogic::isValidBishopMove(const BoardPosition& from, const BoardPosition& to) const {
    // Bishops move diagonally
    const int dx = std::abs(to.first - from.first);
    const int dy = std::abs(to.second - from.second);
    
    // Check if the move is diagonal
    if (dx != dy || dx == 0) {
        return false;
    }
    
    // Check if the path is clear
    const int xDirection = (to.first > from.first) ? 1 : -1;
    const int yDirection = (to.second > from.second) ? 1 : -1;
    
    for (int i = 1; i < dx; ++i) {
        BoardPosition pos(from.first + i * xDirection, from.second + i * yDirection);
        if (pieces.find(pos) != pieces.end()) {
            return false; // Path is blocked
        }
    }
    
    return true;
}

// Check if a rook move is valid
bool ChessGameLogic::isValidRookMove(const BoardPosition& from, const BoardPosition& to) const {
    // Rooks move horizontally or vertically
    const int dx = to.first - from.first;
    const int dy = to.second - from.second;
    
    // Check if the move is horizontal or vertical (but not both)
    if ((dx != 0 && dy != 0) || (dx == 0 && dy == 0)) {
        return false;
    }
    
    // Check if the path is clear
    if (dx != 0) {
        // Horizontal move
        const int direction = (dx > 0) ? 1 : -1;
        for (int i = 1; i < std::abs(dx); ++i) {
            BoardPosition pos(from.first + i * direction, from.second);
            if (pieces.find(pos) != pieces.end()) {
                return false; // Path is blocked
            }
        }
    } else {
        // Vertical move
        const int direction = (dy > 0) ? 1 : -1;
        for (int i = 1; i < std::abs(dy); ++i) {
            BoardPosition pos(from.first, from.second + i * direction);
            if (pieces.find(pos) != pieces.end()) {
                return false; // Path is blocked
            }
        }
    }
    
    return true;
}

// Check if a queen move is valid
bool ChessGameLogic::isValidQueenMove(const BoardPosition& from, const BoardPosition& to) const {
    // Queens can move like a rook or a bishop
    return isValidRookMove(from, to) || isValidBishopMove(from, to);
}

// Check if a king move is valid
bool ChessGameLogic::isValidKingMove(const BoardPosition& from, const BoardPosition& to, 
                                   PieceColor color) const {
    // Normal king move: one square in any direction
    const int dx = std::abs(to.first - from.first);
    const int dy = std::abs(to.second - from.second);
    
    if (dx <= 1 && dy <= 1 && !(dx == 0 && dy == 0)) {
        // Make sure the destination square is not under attack
        PieceColor opponentColor = (color == PieceColor::WHITE) ? 
                                  PieceColor::BLACK : PieceColor::WHITE;
        
        if (isSquareAttacked(to, opponentColor)) {
            return false; // King cannot move to a square under attack
        }
        
        return true;
    }
    
    // Check for castling (2 squares horizontally)
    if (dy == 0 && dx == 2) {
        // Determine direction and check castling rights
        const bool isKingside = (to.first > from.first);
        
        if (isKingside) {
            return canCastleKingside(color);
        } else {
            return canCastleQueenside(color);
        }
    }
    
    return false;
}

// Get all valid moves for a piece at the specified position
std::vector<BoardPosition> ChessGameLogic::getValidMovesForPiece(const BoardPosition& piecePos) const {
    std::vector<BoardPosition> validMoves;
    
    // Check if there's a piece at the specified position
    auto pieceIt = pieces.find(piecePos);
    if (pieceIt == pieces.end() || pieceIt->second.color != currentTurn) {
        return validMoves; // No piece or wrong color
    }
    
    // Check all possible destination squares on the board
    for (int x = 0; x < 8; ++x) {
        for (int y = 0; y < 8; ++y) {
            BoardPosition to(x, y);
            if (isValidMove(piecePos, to)) {
                validMoves.push_back(to);
            }
        }
    }
    
    return validMoves;
}

// Update the getLegalMoves method to use the new implementation
std::vector<BoardPosition> ChessGameLogic::getLegalMoves(const BoardPosition& from) const {
    return getValidMovesForPiece(from);
}

// Check if a player can castle kingside
bool ChessGameLogic::canCastleKingside(PieceColor color) const {
    // Get the king and rook positions
    const int rank = (color == PieceColor::WHITE) ? 7 : 0;
    const BoardPosition kingPos(4, rank);
    const BoardPosition rookPos(7, rank);
    
    // Check if the king and rook are in their initial positions
    auto kingIt = pieces.find(kingPos);
    auto rookIt = pieces.find(rookPos);
    
    if (kingIt == pieces.end() || rookIt == pieces.end()) {
        return false;
    }
    
    if (kingIt->second.type != PieceType::KING || kingIt->second.color != color ||
        rookIt->second.type != PieceType::ROOK || rookIt->second.color != color) {
        return false;
    }
    
    // Check if the squares between the king and rook are empty
    for (int file = 5; file < 7; ++file) {
        BoardPosition pos(file, rank);
        if (pieces.find(pos) != pieces.end()) {
            return false;
        }
    }
    
    // Check if the king is in check or if any squares the king passes through are under attack
    PieceColor opponentColor = (color == PieceColor::WHITE) ? PieceColor::BLACK : PieceColor::WHITE;
    
    // King can't castle out of check
    if (isKingInCheck(color)) {
        return false;
    }
    
    // King can't castle through check
    for (int file = 5; file <= 6; ++file) {
        BoardPosition pos(file, rank);
        if (isSquareAttacked(pos, opponentColor)) {
            return false;
        }
    }
    
    return true;
}

// Check if a player can castle queenside
bool ChessGameLogic::canCastleQueenside(PieceColor color) const {
    // Get the king and rook positions
    const int rank = (color == PieceColor::WHITE) ? 7 : 0;
    const BoardPosition kingPos(4, rank);
    const BoardPosition rookPos(0, rank);
    
    // Check if the king and rook are in their initial positions
    auto kingIt = pieces.find(kingPos);
    auto rookIt = pieces.find(rookPos);
    
    if (kingIt == pieces.end() || rookIt == pieces.end()) {
        return false;
    }
    
    if (kingIt->second.type != PieceType::KING || kingIt->second.color != color ||
        rookIt->second.type != PieceType::ROOK || rookIt->second.color != color) {
        return false;
    }
    
    // Check if the squares between the king and rook are empty
    for (int file = 1; file < 4; ++file) {
        BoardPosition pos(file, rank);
        if (pieces.find(pos) != pieces.end()) {
            return false;
        }
    }
    
    // Check if the king is in check or if any squares the king passes through are under attack
    PieceColor opponentColor = (color == PieceColor::WHITE) ? PieceColor::BLACK : PieceColor::WHITE;
    
    // King can't castle out of check
    if (isKingInCheck(color)) {
        return false;
    }
    
    // King can't castle through check
    for (int file = 2; file <= 3; ++file) {
        BoardPosition pos(file, rank);
        if (isSquareAttacked(pos, opponentColor)) {
            return false;
        }
    }
    
    return true;
}

// Clear the attack cache
// This is already defined in the header file as an inline method
// void ChessGameLogic::clearCache() {
//     attackedSquareCache.clear();
// }
