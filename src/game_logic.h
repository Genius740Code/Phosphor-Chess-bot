#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <map>
#include <vector>
#include <string>
#include <unordered_map>
#include "pieces_placement.h"

// Type definitions for board positions
using BoardPosition = std::pair<int, int>;

// Hash function for BoardPosition
struct BoardPositionHash {
    std::size_t operator()(const BoardPosition& pos) const {
        return std::hash<int>()(pos.first) ^ (std::hash<int>()(pos.second) << 1);
    }
};

// Custom hash key for attacked square cache
struct AttackedSquareKey {
    BoardPosition square;
    PieceColor attackingColor;
    std::size_t boardHash; // Hash of the current board state
    
    bool operator==(const AttackedSquareKey& other) const {
        return square == other.square && 
               attackingColor == other.attackingColor && 
               boardHash == other.boardHash;
    }
};

// Hash function for AttackedSquareKey
struct AttackedSquareKeyHash {
    std::size_t operator()(const AttackedSquareKey& key) const {
        return BoardPositionHash()(key.square) ^ 
               (static_cast<std::size_t>(key.attackingColor) << 16) ^ 
               (key.boardHash << 8);
    }
};

// Enum for game state
enum class GameState {
    ACTIVE,         // Game is in progress
    CHECK,          // Current player is in check
    CHECKMATE,      // Game over by checkmate
    STALEMATE,      // Game over by stalemate (no legal moves but not in check)
    DRAW_FIFTY,     // Game over by fifty-move rule
    DRAW_REPETITION,// Game over by threefold repetition
    DRAW_MATERIAL,  // Game over by insufficient material
    DRAW_AGREEMENT  // Game over by draw agreement
};

// Class to handle chess game logic
class ChessGameLogic {
private:
    // Reference to the board state
    std::map<BoardPosition, ChessPiece>& pieces;
    
    // Game state tracking
    PieceColor currentTurn;
    GameState gameState;
    int halfMoveClock;          // For fifty-move rule (resets on pawn move or capture)
    int fullMoveCounter;        // Increments after Black's move
    
    // Position history for threefold repetition detection
    std::vector<std::string> positionHistory;
    
    // En passant target square
    BoardPosition enPassantTarget;
    bool enPassantAvailable;
    
    // King positions
    BoardPosition whiteKingPosition;
    BoardPosition blackKingPosition;
    
    // Cache for isSquareAttacked calculations
    mutable std::unordered_map<AttackedSquareKey, bool, AttackedSquareKeyHash> attackedSquareCache;
    
    // Calculate a hash of the current board state for cache keys
    std::size_t calculateBoardHash() const;
    
    // Internal helper methods
    void updateKingPositions();
    bool isKingInCheck(PieceColor kingColor) const;
    bool wouldBeInCheck(const BoardPosition& kingPos, PieceColor kingColor, 
                        const std::map<BoardPosition, ChessPiece>& boardState) const;
    bool hasLegalMoves(PieceColor playerColor) const;
    bool hasInsufficientMaterial() const;
    int getRepetitionCount(const std::string& position) const;
    std::string getCurrentPositionFEN() const;
    bool isPiecePinned(const BoardPosition& piecePos, PieceColor pieceColor) const;
    bool isMoveLegal(const BoardPosition& from, const BoardPosition& to) const;
    std::vector<BoardPosition> getValidMovesForPiece(const BoardPosition& piecePos) const;
    bool isValidPawnMove(const BoardPosition& from, const BoardPosition& to, PieceColor color) const;
    bool isValidKnightMove(const BoardPosition& from, const BoardPosition& to) const;
    bool isValidBishopMove(const BoardPosition& from, const BoardPosition& to) const;
    bool isValidRookMove(const BoardPosition& from, const BoardPosition& to) const;
    bool isValidQueenMove(const BoardPosition& from, const BoardPosition& to) const;
    bool isValidKingMove(const BoardPosition& from, const BoardPosition& to, PieceColor color) const;
    bool isAlongLine(const BoardPosition& a, const BoardPosition& b, const BoardPosition& c) const;
    
    // Clears the cache
    void clearCache() {
        attackedSquareCache.clear();
    }

public:
    // Constructor
    ChessGameLogic(std::map<BoardPosition, ChessPiece>& piecesRef);
    
    // Attack checking methods
    bool isSquareAttacked(const BoardPosition& square, PieceColor attackingColor) const;
    bool isSquareAttackedByPieces(const BoardPosition& square, PieceColor attackingColor,
                                  const std::map<BoardPosition, ChessPiece>& boardState) const;
    
    // Game state methods
    PieceColor getCurrentTurn() const { return currentTurn; }
    GameState getGameState() const { return gameState; }
    void switchTurn();
    
    // Position retrieval
    BoardPosition getKingPosition(PieceColor color) const {
        return (color == PieceColor::WHITE) ? whiteKingPosition : blackKingPosition;
    }
    
    // Move validation and execution
    bool isValidMove(const BoardPosition& from, const BoardPosition& to) const;
    void executeMove(const BoardPosition& from, const BoardPosition& to);
    std::vector<BoardPosition> getLegalMoves(const BoardPosition& from) const;
    
    // Check and mate detection
    bool isInCheck() const;
    bool isCheckmate() const;
    bool isStalemate() const;
    
    // Draw conditions
    bool isDraw() const;
    bool isDraw50MoveRule() const { return halfMoveClock >= 100; } // 50 moves = 100 half-moves
    bool isDrawByRepetition() const;
    bool isDrawByInsufficientMaterial() const;
    void offerDraw(bool accepted);
    
    // Special move helpers
    bool canCastleKingside(PieceColor color) const;
    bool canCastleQueenside(PieceColor color) const;
    BoardPosition getEnPassantTarget() const { return enPassantTarget; }
    bool isEnPassantAvailable() const { return enPassantAvailable; }
    
    // Game reset
    void resetGame();
    void setupFromFEN(const std::string& fen);
};

#endif // GAME_LOGIC_H
