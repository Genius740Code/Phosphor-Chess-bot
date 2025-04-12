#include "game_logic.h"

/**
 * @brief Handles user interactions with the chess board
 * 
 * This class manages piece selection, highlighting, and displaying
 * legal moves for selected pieces.
 */
class ChessInteraction {
private:
    // Reference to the board state
    std::map<BoardPosition, ChessPiece>& pieces;
    
    // Reference to game logic
    ChessGameLogic& gameLogic;
    
    // Selection state
    BoardPosition selectedSquare{-1, -1}; // (-1, -1) means no selection
    std::vector<BoardPosition> legalMoves; // Stores legal moves for selected piece

    // ... existing code ...

public:
    /**
     * @brief Constructor
     * @param piecesRef Reference to the map of chess pieces
     * @param gameLogicRef Reference to the game logic handler
     * @param squareSz Size of each square on the chess board
     */
    ChessInteraction(std::map<BoardPosition, ChessPiece>& piecesRef, 
                    ChessGameLogic& gameLogicRef,
                    float squareSz);
    
    // ... existing code ...
    
    /**
     * @brief Gets the current player's turn
     * @return PieceColor of the current player
     */
    PieceColor getCurrentTurn() const { return gameLogic.getCurrentTurn(); }
};

// ... existing code ... 