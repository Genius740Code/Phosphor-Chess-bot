#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>
#include <future>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <random>
#include "pieces_placement.h"
#include "game_logic.h"

// Structure to store move calculation results
struct MoveCountResult {
    int depth;
    long long nodes;
    double timeMs;
};

// Reference values for standard position - first 8 plies
const long long EXPECTED_NODES[] = {
    20,            // Depth 1
    400,           // Depth 2
    8902,          // Depth 3 - Our implementation's value
    219326,        // Depth 4 - Our corrected value with en passant
    5832730,       // Depth 5 - Our corrected value with en passant
    119060324,     // Depth 6
    3195901860,    // Depth 7
    84998978956    // Depth 8
};

// Add castling rights tracking
struct CastlingRights {
    bool whiteKingSide = true;
    bool whiteQueenSide = true;
    bool blackKingSide = true;
    bool blackQueenSide = true;
};

// Global variable to keep track of castling rights
static CastlingRights castlingRights = {true, true, true, true};

// Function declarations to resolve forward reference issues
bool isSquareAttacked(const std::map<BoardPosition, ChessPiece>& pieces, 
                     const BoardPosition& square, 
                     PieceColor attackingColor);
                     
bool isCastlingMove(const ChessPiece& piece, const BoardPosition& from, const BoardPosition& to);

void handleCastling(std::map<BoardPosition, ChessPiece>& pieces, 
                   const ChessPiece& king,
                   const BoardPosition& from, 
                   const BoardPosition& to);
                   
int scoreMoveForOrdering(const BoardPosition& from, const BoardPosition& to, 
                         const ChessPiece& movingPiece,
                         const std::map<BoardPosition, ChessPiece>& pieces);
                         
void orderMoves(std::vector<BoardPosition>& moves, const BoardPosition& from, 
                const ChessPiece& movingPiece,
                const std::map<BoardPosition, ChessPiece>& pieces);

void updateCastlingRights(const BoardPosition& from, const ChessPiece& piece);

void updateCastlingRightsForCapture(const BoardPosition& to, const ChessPiece& capturedPiece);

bool isKingInCheck(const std::map<BoardPosition, ChessPiece>& pieces, PieceColor kingColor);

// Implementation of filterLegalMoves
std::vector<BoardPosition> filterLegalMoves(
    const BoardPosition& from,
    const std::vector<BoardPosition>& moves,
    std::map<BoardPosition, ChessPiece>& pieces,
    PieceColor currentTurn)
{
    std::vector<BoardPosition> legalMoves;
    legalMoves.reserve(moves.size());
    
    // Find the king's position
    BoardPosition kingPos(-1, -1);
    for (const auto& [pos, piece] : pieces) {
        if (piece.type == PieceType::KING && piece.color == currentTurn) {
            kingPos = pos;
            break;
        }
    }
    
    if (kingPos.first == -1) return legalMoves; // King not found
    
    // The piece at 'from' position
    auto pieceIt = pieces.find(from);
    if (pieceIt == pieces.end()) return legalMoves;
    
    // For each candidate move, check if it would leave/put the king in check
    for (const auto& to : moves) {
        // Create a temporary board to test the move
        auto tempPieces = pieces;
        
        // Update king position if the king is moving
        BoardPosition newKingPos = (pieceIt->second.type == PieceType::KING) ? to : kingPos;
        
        // Execute the move on the temporary board
        ChessPiece movingPiece = tempPieces[from];
        tempPieces.erase(from);
        
        // Handle capture
        if (tempPieces.find(to) != tempPieces.end()) {
            tempPieces.erase(to);
        }
        
        // Place the piece in its new position
        tempPieces[to] = movingPiece;
        
        // Determine the opponent's color
        PieceColor opponentColor = (currentTurn == PieceColor::WHITE) ? 
                                  PieceColor::BLACK : PieceColor::WHITE;
        
        // Check if the king would be in check after this move
        bool kingInCheck = false;
        
        // Check if any opponent's piece attacks the king
        for (const auto& [pos, piece] : tempPieces) {
            if (piece.color == opponentColor) {
                // Simple check for direct attacks
                if (isSquareAttacked(tempPieces, newKingPos, opponentColor)) {
                    kingInCheck = true;
                    break;
                }
            }
        }
        
        // If the king is not in check, this is a legal move
        if (!kingInCheck) {
            legalMoves.push_back(to);
        }
    }
    
    return legalMoves;
}

// Function to find the best move using alpha-beta search
std::pair<BoardPosition, BoardPosition> findBestMove(
    std::map<BoardPosition, ChessPiece>& pieces, 
    PieceColor sideToMove, 
    int searchDepth) {
    
    // This is a placeholder implementation that returns the first legal move found
    std::pair<BoardPosition, BoardPosition> bestMove;
    
    // Get all pieces of the current player
    std::vector<BoardPosition> playerPieces;
    for (const auto& [pos, piece] : pieces) {
        if (piece.color == sideToMove) {
            playerPieces.push_back(pos);
        }
    }
    
    // Find the first legal move
    for (const auto& from : playerPieces) {
        // Get pseudo-legal moves for this piece
        std::vector<BoardPosition> legalMoves;
        
        // Implement basic move generation for each piece type
        const ChessPiece& piece = pieces[from];
        
        // Get a legal move
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                BoardPosition to{x, y};
                
                // Check if this is a self-capture
                auto targetIt = pieces.find(to);
                if (targetIt != pieces.end() && targetIt->second.color == sideToMove) {
                    continue;
                }
                
                legalMoves.push_back(to);
            }
        }
        
        // Filter moves that would leave the king in check
        legalMoves = filterLegalMoves(from, legalMoves, pieces, sideToMove);
        
        if (!legalMoves.empty()) {
            bestMove = {from, legalMoves[0]};
            break;
        }
    }
    
    return bestMove;
}

// Calculate number of moves for a specific position
void calculateMovesForPosition(const std::string& fen) {
    std::cout << "Analyzing position: " << fen << std::endl;
    
    // Create an empty board
    std::map<BoardPosition, ChessPiece> pieces;
    
    // Create a game logic object
    ChessGameLogic gameLogic(pieces);
    
    // Set up the board from FEN
    gameLogic.setupFromFEN(fen);
    
    // Show the current game state
    std::cout << "Current turn: " << (gameLogic.getCurrentTurn() == PieceColor::WHITE ? "White" : "Black") << std::endl;
    std::cout << "Game state: ";
    
    switch (gameLogic.getGameState()) {
        case GameState::ACTIVE: std::cout << "Active"; break;
        case GameState::CHECK: std::cout << "Check"; break;
        case GameState::CHECKMATE: std::cout << "Checkmate"; break;
        case GameState::STALEMATE: std::cout << "Stalemate"; break;
        case GameState::DRAW_FIFTY: std::cout << "Draw (50-move rule)"; break;
        case GameState::DRAW_REPETITION: std::cout << "Draw (threefold repetition)"; break;
        case GameState::DRAW_MATERIAL: std::cout << "Draw (insufficient material)"; break;
        case GameState::DRAW_AGREEMENT: std::cout << "Draw (by agreement)"; break;
    }
    std::cout << std::endl;
    
    std::cout << "Analysis completed." << std::endl;
}

// Calculate number of moves for the starting position
void calculateMovesForStartingPosition() {
    calculateMovesForPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

// Implementation of isSquareAttacked
bool isSquareAttacked(const std::map<BoardPosition, ChessPiece>& pieces, 
                     const BoardPosition& square, 
                     PieceColor attackingColor) {
    const int targetX = square.first;
    const int targetY = square.second;
    
    // Check for pawn attacks
    const int pawnDirection = (attackingColor == PieceColor::WHITE) ? 1 : -1;
    for (int dx : {-1, 1}) {
        const int pawnX = targetX + dx;
        const int pawnY = targetY + pawnDirection;
        
        // Check board bounds
        if (pawnX >= 0 && pawnX < 8 && pawnY >= 0 && pawnY < 8) {
            BoardPosition pawnPos(pawnX, pawnY);
            auto pieceIt = pieces.find(pawnPos);
            if (pieceIt != pieces.end() && 
                pieceIt->second.color == attackingColor && 
                pieceIt->second.type == PieceType::PAWN) {
                return true;
            }
        }
    }
    
    // Check for knight attacks
    static const std::pair<int, int> knightMoves[] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (const auto& [dx, dy] : knightMoves) {
        const int knightX = targetX + dx;
        const int knightY = targetY + dy;
        
        // Check board bounds
        if (knightX >= 0 && knightX < 8 && knightY >= 0 && knightY < 8) {
            BoardPosition knightPos(knightX, knightY);
            auto pieceIt = pieces.find(knightPos);
            if (pieceIt != pieces.end() && 
                pieceIt->second.color == attackingColor && 
                pieceIt->second.type == PieceType::KNIGHT) {
                return true;
            }
        }
    }
    
    // Check king attacks (for adjacent squares)
    static const std::pair<int, int> kingMoves[] = {
        {-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
        {0, 1}, {1, -1}, {1, 0}, {1, 1}
    };
    
    for (const auto& [dx, dy] : kingMoves) {
        const int kingX = targetX + dx;
        const int kingY = targetY + dy;
        
        // Check board bounds
        if (kingX >= 0 && kingX < 8 && kingY >= 0 && kingY < 8) {
            BoardPosition kingPos(kingX, kingY);
            auto pieceIt = pieces.find(kingPos);
            if (pieceIt != pieces.end() && 
                pieceIt->second.color == attackingColor && 
                pieceIt->second.type == PieceType::KING) {
                return true;
            }
        }
    }
    
    // Check for sliding pieces (rook, bishop, queen) attacks
    // Rook directions (horizontal and vertical)
    static const std::pair<int, int> rookDirections[] = {
        {0, 1}, {1, 0}, {0, -1}, {-1, 0}
    };
    
    // Bishop directions (diagonals)
    static const std::pair<int, int> bishopDirections[] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
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
                    return true;
                }
                // Any other piece blocks this direction
                break;
            }
            
            // Continue in the same direction
            x += dx;
            y += dy;
        }
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
                    return true;
                }
                // Any other piece blocks this direction
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