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
    8902,          // Depth 3
    197281,        // Depth 4
    4865609,       // Depth 5
    119060324,     // Depth 6
    3195901860,    // Depth 7
    84998978956    // Depth 8
};

// Use the BoardPositionHash from game_logic.h instead of redefining it

// Structure to hold a move with source and destination
struct ChessMove {
    BoardPosition from;
    BoardPosition to;
    
    bool operator==(const ChessMove& other) const {
        return from == other.from && to == other.to;
    }
};

// Cache key for move generation
struct MoveGenKey {
    BoardPosition position;
    PieceType pieceType;
    PieceColor pieceColor;
    
    bool operator==(const MoveGenKey& other) const {
        return position == other.position && 
               pieceType == other.pieceType && 
               pieceColor == other.pieceColor;
    }
};

// Hash function for MoveGenKey
struct MoveGenKeyHash {
    std::size_t operator()(const MoveGenKey& key) const {
        return BoardPositionHash()(key.position) ^ 
               (static_cast<std::size_t>(key.pieceType) << 16) ^ 
               (static_cast<std::size_t>(key.pieceColor) << 19);
    }
};

// A compact board representation for quicker copying
class CompactBoard {
private:
    // 8x8 board with piece type (3 bits) and color (1 bit)
    std::array<uint8_t, 64> board;
    
public:
    CompactBoard() {
        std::fill(board.begin(), board.end(), 0);
    }
    
    // Convert from standard map representation
    void fromPiecesMap(const std::map<BoardPosition, ChessPiece>& pieces) {
        std::fill(board.begin(), board.end(), 0);
        
        for (const auto& [pos, piece] : pieces) {
            if (pos.first >= 0 && pos.first < 8 && pos.second >= 0 && pos.second < 8) {
                // Encode piece: 0=empty, 1-6=piece types, bit 4 = color (0=white, 1=black)
                uint8_t pieceValue = static_cast<uint8_t>(piece.type) + 1;
                if (piece.color == PieceColor::BLACK) {
                    pieceValue |= 0x08; // Set color bit
                }
                
                board[pos.second * 8 + pos.first] = pieceValue;
            }
        }
    }
    
    // Make a move on the compact board
    void makeMove(const BoardPosition& from, const BoardPosition& to) {
        int fromIdx = from.second * 8 + from.first;
        int toIdx = to.second * 8 + to.first;
        
        if (fromIdx >= 0 && fromIdx < 64 && toIdx >= 0 && toIdx < 64) {
            board[toIdx] = board[fromIdx];
            board[fromIdx] = 0;
        }
    }
    
    // Get piece at position
    std::pair<PieceType, PieceColor> getPieceAt(const BoardPosition& pos) const {
        int idx = pos.second * 8 + pos.first;
        if (idx >= 0 && idx < 64) {
            uint8_t value = board[idx];
            if (value == 0) {
                // No piece
                return {PieceType::PAWN, PieceColor::WHITE}; // Default, unused
            }
            
            PieceType type = static_cast<PieceType>((value & 0x07) - 1);
            PieceColor color = (value & 0x08) ? PieceColor::BLACK : PieceColor::WHITE;
            
            return {type, color};
        }
        
        // Default return for invalid positions
        return {PieceType::PAWN, PieceColor::WHITE};
    }
    
    // Check if position has a piece
    bool hasPiece(const BoardPosition& pos) const {
        int idx = pos.second * 8 + pos.first;
        return (idx >= 0 && idx < 64 && board[idx] != 0);
    }
    
    // Check if position has a piece of specific color
    bool hasPieceOfColor(const BoardPosition& pos, PieceColor color) const {
        int idx = pos.second * 8 + pos.first;
        if (idx >= 0 && idx < 64 && board[idx] != 0) {
            return (board[idx] & 0x08) ? (color == PieceColor::BLACK) : (color == PieceColor::WHITE);
        }
        return false;
    }
    
    // Get hash of board state
    std::size_t getHash() const {
        std::size_t hash = 0;
        for (size_t i = 0; i < 64; ++i) {
            hash = hash * 31 + board[i];
        }
        return hash;
    }
};

// Forward declarations
long long countMovesAtDepth(int depth, std::map<BoardPosition, ChessPiece>& pieces, PieceColor currentTurn);
long long countMovesParallel(int depth, const std::map<BoardPosition, ChessPiece>& initialPieces, PieceColor currentTurn);
std::vector<BoardPosition> getAllPiecesForColor(const std::map<BoardPosition, ChessPiece>& pieces, PieceColor color);

// Move generation with caching
std::vector<BoardPosition> getLegalMovesForPiece(
    const BoardPosition& from, 
    std::map<BoardPosition, ChessPiece>& pieces, 
    PieceColor currentTurn);

// Global move generation cache
static std::unordered_map<MoveGenKey, std::vector<BoardPosition>, MoveGenKeyHash> moveGenCache;
static std::mutex cacheMutex;

// Debug function to print the board
void printBoard(const std::map<BoardPosition, ChessPiece>& pieces) {
    std::cout << "  +---+---+---+---+---+---+---+---+" << std::endl;
    for (int row = 0; row < 8; row++) {
        std::cout << (8 - row) << " |";
        for (int col = 0; col < 8; col++) {
            BoardPosition pos(col, row);
            auto it = pieces.find(pos);
            char piece = ' ';
            if (it != pieces.end()) {
                switch (it->second.type) {
                    case PieceType::PAWN:   piece = 'P'; break;
                    case PieceType::ROOK:   piece = 'R'; break;
                    case PieceType::KNIGHT: piece = 'N'; break;
                    case PieceType::BISHOP: piece = 'B'; break;
                    case PieceType::QUEEN:  piece = 'Q'; break;
                    case PieceType::KING:   piece = 'K'; break;
                }
                if (it->second.color == PieceColor::BLACK) {
                    piece = std::tolower(piece);
                }
            }
            std::cout << " " << piece << " |";
        }
        std::cout << std::endl;
        std::cout << "  +---+---+---+---+---+---+---+---+" << std::endl;
    }
    std::cout << "    a   b   c   d   e   f   g   h  " << std::endl;
}

// Multi-threaded version for higher depths
long long countMovesParallel(int depth, const std::map<BoardPosition, ChessPiece>& initialPieces, PieceColor currentTurn) {
    if (depth <= 3) {
        // Use single-threaded version for shallow depths
        std::map<BoardPosition, ChessPiece> pieces = initialPieces;
        return countMovesAtDepth(depth, pieces, currentTurn);
    }
    
    // Get all pieces of the current player
    std::vector<BoardPosition> playerPieces = getAllPiecesForColor(initialPieces, currentTurn);
    
    // Calculate optimal number of threads based on hardware
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4; // Default if hardware_concurrency fails
    
    // Cap threads to avoid overhead with too many threads
    numThreads = std::min(numThreads, 16u);
    
    // Adjust based on depth - use fewer threads for moderate depths to reduce overhead
    if (depth <= 5) {
        numThreads = std::min(numThreads, 8u);
    }
    
    std::vector<std::future<long long>> futures;
    long long totalMoves = 0;
    
    // Function to process a subset of root moves
    auto processRootMoves = [&](const std::vector<BoardPosition>& rootMoves) -> long long {
        long long subTotal = 0;
        std::map<BoardPosition, ChessPiece> threadPieces = initialPieces;
        
        for (const auto& from : rootMoves) {
            // Get legal moves for this piece
            std::vector<BoardPosition> moves = getLegalMovesForPiece(from, threadPieces, currentTurn);
            
            // Skip if no moves
            if (moves.empty()) continue;
            
            // Cache the moving piece
            ChessPiece movingPiece = threadPieces[from];
            
            // Pre-calculate the next player's turn
            PieceColor nextTurn = (currentTurn == PieceColor::WHITE) ? 
                              PieceColor::BLACK : PieceColor::WHITE;
            
            for (const auto& to : moves) {
                // Remember captured piece (if any)
                auto targetIt = threadPieces.find(to);
                bool hadPiece = (targetIt != threadPieces.end());
                ChessPiece capturedPiece;
                if (hadPiece) {
                    capturedPiece = targetIt->second;
                }
                
                // Make the move
                threadPieces[to] = movingPiece;
                threadPieces.erase(from);
                
                // Recursively count moves for the next player
                subTotal += countMovesAtDepth(depth - 1, threadPieces, nextTurn);
                
                // Unmake the move
                threadPieces[from] = movingPiece;
                if (hadPiece) {
                    threadPieces[to] = capturedPiece;
                } else {
                    threadPieces.erase(to);
                }
            }
        }
        
        return subTotal;
    };
    
    // Distribute root moves among threads
    if (!playerPieces.empty()) {
        // Try to evenly distribute work by pre-calculating moves per piece
        std::vector<std::pair<BoardPosition, size_t>> piecesWithMoveCount;
        piecesWithMoveCount.reserve(playerPieces.size());
        
        std::map<BoardPosition, ChessPiece> tempPieces = initialPieces;
        for (const auto& pos : playerPieces) {
            auto moves = getLegalMovesForPiece(pos, tempPieces, currentTurn);
            piecesWithMoveCount.push_back({pos, moves.size()});
        }
        
        // Sort pieces by move count (descending) for better load balancing
        std::sort(piecesWithMoveCount.begin(), piecesWithMoveCount.end(), 
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Assign pieces to threads round-robin style
        std::vector<std::vector<BoardPosition>> threadAssignments(numThreads);
        for (size_t i = 0; i < piecesWithMoveCount.size(); ++i) {
            threadAssignments[i % numThreads].push_back(piecesWithMoveCount[i].first);
        }
        
        // Launch threads
        for (unsigned int i = 0; i < numThreads; ++i) {
            if (!threadAssignments[i].empty()) {
                futures.push_back(std::async(std::launch::async, processRootMoves, threadAssignments[i]));
            }
        }
        
        // Collect results
        for (auto& future : futures) {
            totalMoves += future.get();
        }
    }
    
    return totalMoves;
}

// Main function to calculate moves for a given FEN position
void calculateMovesForPosition(const std::string& fen) {
    // Setup position
    std::map<BoardPosition, ChessPiece> pieces;
    
    // Need to load textures first
    if (!loadPieceTextures(1.0f)) {
        std::cout << "Warning: Failed to load piece textures, but we can still count moves." << std::endl;
    }
    
    // Use empty string to indicate standard starting position
    std::string fenToUse = fen.empty() ? 
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" : fen;
    
    setupPositionFromFEN(pieces, fenToUse);
    
    // Print the board to verify it's set up correctly
    std::cout << "Board position:" << std::endl;
    printBoard(pieces);
    
    // Count pieces to verify
    int whitePieces = 0, blackPieces = 0;
    for (const auto& [pos, piece] : pieces) {
        if (piece.color == PieceColor::WHITE) {
            whitePieces++;
        } else {
            blackPieces++;
        }
    }
    std::cout << "White pieces: " << whitePieces << ", Black pieces: " << blackPieces << std::endl;
    
    // Extract the current turn from FEN
    PieceColor currentTurn = PieceColor::WHITE;  // Default to white
    size_t spacePos = fenToUse.find(' ');
    if (spacePos != std::string::npos && spacePos + 1 < fenToUse.length()) {
        char turnChar = fenToUse[spacePos + 1];
        if (turnChar == 'b') {
            currentTurn = PieceColor::BLACK;
        }
    }
    std::cout << "Current turn: " << (currentTurn == PieceColor::WHITE ? "WHITE" : "BLACK") << std::endl;
    
    // Get all pieces for the current color
    std::vector<BoardPosition> playerPieces = getAllPiecesForColor(pieces, currentTurn);
    std::cout << "Found " << playerPieces.size() << " pieces for the current player" << std::endl;
    
    // Print each piece and its legal moves
    for (const auto& pos : playerPieces) {
        std::cout << "Piece at (" << pos.first << "," << pos.second << "): ";
        auto it = pieces.find(pos);
        if (it != pieces.end()) {
            switch (it->second.type) {
                case PieceType::PAWN:   std::cout << "PAWN"; break;
                case PieceType::ROOK:   std::cout << "ROOK"; break;
                case PieceType::KNIGHT: std::cout << "KNIGHT"; break;
                case PieceType::BISHOP: std::cout << "BISHOP"; break;
                case PieceType::QUEEN:  std::cout << "QUEEN"; break;
                case PieceType::KING:   std::cout << "KING"; break;
            }
            std::cout << " (" << (it->second.color == PieceColor::WHITE ? "WHITE" : "BLACK") << ")" << std::endl;
            
            // Get legal moves for this piece
            std::vector<BoardPosition> moves = getLegalMovesForPiece(pos, pieces, currentTurn);
            std::cout << "  Legal moves: " << moves.size() << std::endl;
            
            // Print each legal move (only for a reasonable number of moves)
            if (moves.size() < 10) {
                for (const auto& move : moves) {
                    std::cout << "    -> (" << move.first << "," << move.second << ")" << std::endl;
                }
            }
        }
    }
    
    // Ask user for maximum depth
    int maxDepth = 4; // Default
    std::cout << "\nEnter maximum depth to calculate (1-8, higher numbers take longer): ";
    std::cin >> maxDepth;
    
    // Validate input
    if (maxDepth < 1) {
        maxDepth = 1;
        std::cout << "Using minimum depth of 1." << std::endl;
    } else if (maxDepth > 8) {
        std::cout << "Warning: Depths > 8 may take a very long time to calculate." << std::endl;
        std::cout << "Are you sure you want to continue with depth " << maxDepth << "? (y/n): ";
        char confirm;
        std::cin >> confirm;
        if (tolower(confirm) != 'y') {
            maxDepth = 8;
            std::cout << "Using maximum depth of 8." << std::endl;
        }
    }
    
    // Store results for each depth
    std::vector<MoveCountResult> results;
    
    std::cout << "\nCalculating moves for position: " << fenToUse << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Clear the move generation cache before search
    moveGenCache.clear();
    
    // Get number of hardware threads
    unsigned int numThreads = std::thread::hardware_concurrency();
    std::cout << "Running on " << (numThreads == 0 ? 4 : numThreads) << " hardware threads" << std::endl;
    
    // Calculate for depths 1 to maxDepth
    for (int depth = 1; depth <= maxDepth; depth++) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Count moves at this depth - using multithreading for depth 4+
        long long moveCount;
        if (depth >= 4) {
            moveCount = countMovesParallel(depth, pieces, currentTurn);
        } else {
            moveCount = countMovesAtDepth(depth, pieces, currentTurn);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Store result
        results.push_back({depth, moveCount, static_cast<double>(duration.count())});
        
        // Print immediate result
        std::cout << "Depth " << depth << ": " << moveCount << " moves";
        std::cout << " (calculated in " << std::fixed << std::setprecision(2) 
                  << duration.count() / 1000.0 << " seconds)";
        
        // Compare with expected values for standard position
        if (fen.empty() && depth <= 8) {
            std::string match = (moveCount == EXPECTED_NODES[depth-1]) ? "CORRECT" : "INCORRECT";
            std::cout << " - " << match << " (expected: " << EXPECTED_NODES[depth-1] << ")";
        }
        
        std::cout << std::endl;
    }
    
    // Print summary table
    std::cout << "\nSummary:" << std::endl;
    std::cout << "-------" << std::endl;
    std::cout << std::setw(8) << "Depth" << std::setw(20) << "Nodes" 
              << std::setw(15) << "Time (sec)" << std::setw(15) << "Nodes/sec";
    
    if (fen.empty()) {
        std::cout << std::setw(20) << "Expected";
    }
    
    std::cout << std::endl;
    
    for (const auto& result : results) {
        double timeInSec = result.timeMs / 1000.0;
        double nodesPerSec = (timeInSec > 0) ? result.nodes / timeInSec : 0;
        
        std::cout << std::setw(8) << result.depth 
                  << std::setw(20) << result.nodes 
                  << std::setw(15) << std::fixed << std::setprecision(2) << timeInSec
                  << std::setw(15) << std::fixed << std::setprecision(0) << nodesPerSec;
        
        // Show expected values for standard position
        if (fen.empty() && result.depth <= 8) {
            std::cout << std::setw(20) << EXPECTED_NODES[result.depth-1];
        }
        
        std::cout << std::endl;
    }
    
    // Clear the move generation cache after search
    moveGenCache.clear();
}

// Calculate moves for the standard starting position
void calculateMovesForStartingPosition() {
    calculateMovesForPosition("");
}

// Simple implementation to get legal moves for a piece
std::vector<BoardPosition> getLegalMovesForPiece(const BoardPosition& from, 
                                               std::map<BoardPosition, ChessPiece>& pieces, 
                                               PieceColor currentTurn) {
    auto pieceIt = pieces.find(from);
    if (pieceIt == pieces.end() || pieceIt->second.color != currentTurn) {
        return std::vector<BoardPosition>();
    }
    
    const ChessPiece& piece = pieceIt->second;
    
    // Check the cache first
    MoveGenKey cacheKey{from, piece.type, piece.color};
    
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto cacheIt = moveGenCache.find(cacheKey);
        if (cacheIt != moveGenCache.end()) {
            return cacheIt->second;
        }
    }
    
    std::vector<BoardPosition> legalMoves;
    legalMoves.reserve(28); // Maximum possible moves for a queen
    
    const int col = from.first;
    const int row = from.second;
    
    // Based on piece type, generate moves
    switch (piece.type) {
        case PieceType::PAWN: {
            // White pawns move up the board (decreasing row)
            // Black pawns move down the board (increasing row)
            int direction = (piece.color == PieceColor::WHITE) ? -1 : 1;
            int startRow = (piece.color == PieceColor::WHITE) ? 6 : 1;
            
            // Forward one square - avoid bounds checking in hot loop by checking once
            int newRow = row + direction;
            if (newRow >= 0 && newRow < 8) {
                BoardPosition forward(col, newRow);
                if (pieces.find(forward) == pieces.end()) {
                    legalMoves.push_back(forward);
                    
                    // Forward two squares from starting position
                    if (row == startRow) {
                        int doubleRow = row + 2 * direction;
                        if (doubleRow >= 0 && doubleRow < 8) {
                            BoardPosition doubleForward(col, doubleRow);
                            if (pieces.find(doubleForward) == pieces.end()) {
                                legalMoves.push_back(doubleForward);
                            }
                        }
                    }
                }
                
                // Capture diagonally left
                if (col > 0) {
                    BoardPosition captureLeft(col - 1, newRow);
                    auto targetPiece = pieces.find(captureLeft);
                    if (targetPiece != pieces.end() && targetPiece->second.color != piece.color) {
                        legalMoves.push_back(captureLeft);
                    }
                }
                
                // Capture diagonally right
                if (col < 7) {
                    BoardPosition captureRight(col + 1, newRow);
                    auto targetPiece = pieces.find(captureRight);
                    if (targetPiece != pieces.end() && targetPiece->second.color != piece.color) {
                        legalMoves.push_back(captureRight);
                    }
                }
            }
            break;
        }
        
        case PieceType::KNIGHT: {
            // Eight possible knight moves - unroll for performance
            static const int knightDx[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
            static const int knightDy[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
            
            for (int i = 0; i < 8; ++i) {
                int newCol = col + knightDx[i];
                int newRow = row + knightDy[i];
                
                // Check if the new position is on the board
                if (newCol >= 0 && newCol < 8 && newRow >= 0 && newRow < 8) {
                    BoardPosition newPos(newCol, newRow);
                    auto targetPiece = pieces.find(newPos);
                    
                    // Empty square or enemy piece
                    if (targetPiece == pieces.end() || targetPiece->second.color != piece.color) {
                        legalMoves.push_back(newPos);
                    }
                }
            }
            break;
        }
        
        case PieceType::BISHOP: {
            // Four diagonal directions - use separate loops to avoid bounds checking
            static const int bishopDx[4] = {-1, -1, 1, 1};
            static const int bishopDy[4] = {-1, 1, -1, 1};
            
            for (int dir = 0; dir < 4; ++dir) {
                int dx = bishopDx[dir];
                int dy = bishopDy[dir];
                
                for (int i = 1; i < 8; ++i) {
                    int newCol = col + i * dx;
                    int newRow = row + i * dy;
                    
                    // Check if new position is on the board
                    if (newCol < 0 || newCol >= 8 || newRow < 0 || newRow >= 8) break;
                    
                    BoardPosition newPos(newCol, newRow);
                    auto targetPiece = pieces.find(newPos);
                    
                    // Empty square
                    if (targetPiece == pieces.end()) {
                        legalMoves.push_back(newPos);
                        continue;
                    }
                    
                    // Enemy piece (capture and stop)
                    if (targetPiece->second.color != piece.color) {
                        legalMoves.push_back(newPos);
                    }
                    
                    // Stop after any piece (friend or enemy)
                    break;
                }
            }
            break;
        }
        
        case PieceType::ROOK: {
            // Four orthogonal directions - use separate arrays for better vectorization
            static const int rookDx[4] = {0, 0, -1, 1};
            static const int rookDy[4] = {-1, 1, 0, 0};
            
            for (int dir = 0; dir < 4; ++dir) {
                int dx = rookDx[dir];
                int dy = rookDy[dir];
                
                for (int i = 1; i < 8; ++i) {
                    int newCol = col + i * dx;
                    int newRow = row + i * dy;
                    
                    // Check if new position is on the board
                    if (newCol < 0 || newCol >= 8 || newRow < 0 || newRow >= 8) break;
                    
                    BoardPosition newPos(newCol, newRow);
                    auto targetPiece = pieces.find(newPos);
                    
                    // Empty square
                    if (targetPiece == pieces.end()) {
                        legalMoves.push_back(newPos);
                        continue;
                    }
                    
                    // Enemy piece (capture and stop)
                    if (targetPiece->second.color != piece.color) {
                        legalMoves.push_back(newPos);
                    }
                    
                    // Stop after any piece (friend or enemy)
                    break;
                }
            }
            break;
        }
        
        case PieceType::QUEEN: {
            // Eight directions (combination of rook and bishop)
            static const int queenDx[8] = {0, 0, -1, 1, -1, -1, 1, 1};
            static const int queenDy[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
            
            for (int dir = 0; dir < 8; ++dir) {
                int dx = queenDx[dir];
                int dy = queenDy[dir];
                
                for (int i = 1; i < 8; ++i) {
                    int newCol = col + i * dx;
                    int newRow = row + i * dy;
                    
                    // Check if new position is on the board
                    if (newCol < 0 || newCol >= 8 || newRow < 0 || newRow >= 8) break;
                    
                    BoardPosition newPos(newCol, newRow);
                    auto targetPiece = pieces.find(newPos);
                    
                    // Empty square
                    if (targetPiece == pieces.end()) {
                        legalMoves.push_back(newPos);
                        continue;
                    }
                    
                    // Enemy piece (capture and stop)
                    if (targetPiece->second.color != piece.color) {
                        legalMoves.push_back(newPos);
                    }
                    
                    // Stop after any piece (friend or enemy)
                    break;
                }
            }
            break;
        }
        
        case PieceType::KING: {
            // Eight directions for one square - unroll for performance
            static const int kingDx[8] = {0, 0, -1, 1, -1, -1, 1, 1};
            static const int kingDy[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
            
            for (int dir = 0; dir < 8; ++dir) {
                int newCol = col + kingDx[dir];
                int newRow = row + kingDy[dir];
                
                // Check if the new position is on the board
                if (newCol >= 0 && newCol < 8 && newRow >= 0 && newRow < 8) {
                    BoardPosition newPos(newCol, newRow);
                    auto targetPiece = pieces.find(newPos);
                    
                    // Empty square or enemy piece
                    if (targetPiece == pieces.end() || targetPiece->second.color != piece.color) {
                        legalMoves.push_back(newPos);
                    }
                }
            }
            
            // We're not implementing castling, en passant, or check detection
            // for simplicity in this move counter
            break;
        }
    }
    
    // Store in cache
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        // Only cache if cache isn't too large to prevent memory growth
        if (moveGenCache.size() < 10000) {
            moveGenCache[cacheKey] = legalMoves;
        }
    }
    
    return legalMoves;
}

// Recursive function to count moves at a specific depth
long long countMovesAtDepth(int depth, std::map<BoardPosition, ChessPiece>& pieces, PieceColor currentTurn) {
    // Base case: at depth 0, return 1 (counted as one position)
    if (depth <= 0) return 1;
    
    long long totalMoves = 0;
    
    // Get all pieces of the current player
    std::vector<BoardPosition> playerPieces = getAllPiecesForColor(pieces, currentTurn);
    
    // Reserve space for moves to avoid reallocations
    std::vector<BoardPosition> moves;
    moves.reserve(28); // Maximum possible moves for a queen
    
    // First depth just reports total number of legal moves
    if (depth == 1) {
        for (const auto& from : playerPieces) {
            moves.clear();
            moves = getLegalMovesForPiece(from, pieces, currentTurn);
            totalMoves += moves.size();
        }
        return totalMoves;
    }
    
    // Pre-calculate the next player's turn to avoid redundant calculations
    PieceColor nextTurn = (currentTurn == PieceColor::WHITE) ? 
                          PieceColor::BLACK : PieceColor::WHITE;
    
    // For deeper depths, we need to make each move and recurse
    for (const auto& from : playerPieces) {
        moves.clear();
        moves = getLegalMovesForPiece(from, pieces, currentTurn);
        
        // Skip if no moves
        if (moves.empty()) continue;
        
        // Cache the moving piece
        ChessPiece movingPiece = pieces[from];
        
        for (const auto& to : moves) {
            // Remember captured piece (if any)
            auto targetIt = pieces.find(to);
            bool hadPiece = (targetIt != pieces.end());
            ChessPiece capturedPiece;
            if (hadPiece) {
                capturedPiece = targetIt->second;
            }
            
            // Make the move (use direct assignment for better performance)
            pieces[to] = movingPiece;
            pieces.erase(from);
            
            // Recursively count moves for the next player
            totalMoves += countMovesAtDepth(depth - 1, pieces, nextTurn);
            
            // Unmake the move (restore the board state)
            pieces[from] = movingPiece;
            if (hadPiece) {
                pieces[to] = capturedPiece;
            } else {
                pieces.erase(to);
            }
        }
    }
    
    return totalMoves;
}

// Helper function to get all pieces of a specific color
std::vector<BoardPosition> getAllPiecesForColor(const std::map<BoardPosition, ChessPiece>& pieces, 
                                              PieceColor color) {
    std::vector<BoardPosition> result;
    result.reserve(16); // Maximum number of pieces of one color
    
    for (const auto& [pos, piece] : pieces) {
        if (piece.color == color) {
            result.push_back(pos);
        }
    }
    
    return result;
} 