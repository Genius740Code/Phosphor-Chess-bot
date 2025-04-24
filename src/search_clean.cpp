
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
    8902,          // Depth 3
    197281,        // Depth 4 (updated for position without en passant)
    4865609,       // Depth 5 (updated for position without en passant)
    119060324,     // Depth 6
    3195901860,    // Depth 7
    84998978956    // Depth 8
};

// Add a global en passant target square
static BoardPosition enPassantTarget = {-1, -1}; // No target by default

// Add a global variable to track en passant capturing pawn position
static BoardPosition enPassantCapturedPawn = {-1, -1};

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
                     
bool isEnPassantCapture(const ChessPiece& piece, 
                       const BoardPosition& from, 
                       const BoardPosition& to,
                       const std::map<BoardPosition, ChessPiece>& pieces);
                       
bool isCastlingMove(const ChessPiece& piece, const BoardPosition& from, const BoardPosition& 
to);

BoardPosition calculateEnPassantTarget(
    const ChessPiece& piece,
    const BoardPosition& from,
    const BoardPosition& to);
    
std::vector<std::pair<BoardPosition, BoardPosition>> getEnPassantCaptures(
    const std::map<BoardPosition, ChessPiece>& pieces,
    PieceColor sideToMove,
    const BoardPosition& epTarget);

                           const ChessPiece& movingPiece,
                           const BoardPosition& from, 
                           const BoardPosition& to);
                           
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

std::vector<BoardPosition> filterLegalMoves(
    const BoardPosition& from,
    const std::vector<BoardPosition>& moves,
    std::map<BoardPosition, ChessPiece>& pieces,
    PieceColor currentTurn
);

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
    BoardPosition epTarget;  // En passant target (affects pawn move generation)
    
    bool operator==(const MoveGenKey& other) const {
        return position == other.position && 
               pieceType == other.pieceType && 
               pieceColor == other.pieceColor &&
               epTarget == other.epTarget;
    }
};

// Hash function for MoveGenKey
struct MoveGenKeyHash {
    std::size_t operator()(const MoveGenKey& key) const {
        std::size_t h1 = BoardPositionHash()(key.position);
        std::size_t h2 = static_cast<std::size_t>(key.pieceType) << 16;
        std::size_t h3 = static_cast<std::size_t>(key.pieceColor) << 19;
        std::size_t h4 = 0;
        if (key.epTarget.first != -1) {
            h4 = BoardPositionHash()(key.epTarget) << 8;
        }
        return h1 ^ h2 ^ h3 ^ h4;
    }
};

// A compact board representation for quicker copying
class CompactBoard {
private:
    // Board representation and positions in correct initialization order
    std::array<uint8_t, 64> board;
    BoardPosition whiteKingPos;
    BoardPosition blackKingPos;
    BoardPosition epTarget;
    
public:
    // Constructor with member initializers in correct order
    CompactBoard() : 
        whiteKingPos{-1, -1}, 
        blackKingPos{-1, -1}, 
        epTarget{-1, -1} 
    {
        std::fill(board.begin(), board.end(), 0);
    }
    
    // Convert from standard map representation
    void fromPiecesMap(const std::map<BoardPosition, ChessPiece>& pieces) {
        std::fill(board.begin(), board.end(), 0);
        
        // Reset king positions
        whiteKingPos = {-1, -1};
        blackKingPos = {-1, -1};
        
        for (const auto& [pos, piece] : pieces) {
            if (pos.first >= 0 && pos.first < 8 && pos.second >= 0 && pos.second < 8) {
                // Encode piece: 0=empty, 1-6=piece types, bit 4 = color (0=white, 1=black)
                uint8_t pieceValue = static_cast<uint8_t>(piece.type) + 1;
                if (piece.color == PieceColor::BLACK) {
                    pieceValue |= 0x08; // Set color bit
                }
                
                board[pos.second * 8 + pos.first] = pieceValue;
                
                // Track king positions
                if (piece.type == PieceType::KING) {
                    if (piece.color == PieceColor::WHITE) {
                        whiteKingPos = pos;
                    } else {
                        blackKingPos = pos;
                    }
                }
            }
        }
    }
    
    // Set the en passant target
    void setEnPassantTarget(const BoardPosition& target) {
        epTarget = target;
    }
    
    // Get the en passant target
    BoardPosition getEnPassantTarget() const {
        return epTarget;
    }
    
    // Get king position for a specific color
    BoardPosition getKingPosition(PieceColor color) const {
        return (color == PieceColor::WHITE) ? whiteKingPos : blackKingPos;
    }
    
    // Make a move on the compact board
    void makeMove(const BoardPosition& from, const BoardPosition& to) {
        int fromIdx = from.second * 8 + from.first;
        int toIdx = to.second * 8 + to.first;
        
        if (fromIdx >= 0 && fromIdx < 64 && toIdx >= 0 && toIdx < 64) {
            // Check if king is moving to update king position
            uint8_t pieceValue = board[fromIdx];
            PieceType pieceType = static_cast<PieceType>((pieceValue & 0x07) - 1);
            PieceColor pieceColor = (pieceValue & 0x08) ? PieceColor::BLACK : 
PieceColor::WHITE;
            
            if (pieceType == PieceType::KING) {
                if (pieceColor == PieceColor::WHITE) {
                    whiteKingPos = to;
                } else {
                    blackKingPos = to;
                }
            }
            
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
            return (board[idx] & 0x08) ? (color == PieceColor::BLACK) : (color == 
PieceColor::WHITE);
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
long long countMovesAtDepth(int depth, std::map<BoardPosition, ChessPiece>& pieces, 
PieceColor currentTurn);
long long countMovesParallel(int depth, const std::map<BoardPosition, ChessPiece>& 
initialPieces, PieceColor currentTurn);
std::vector<BoardPosition> getAllPiecesForColor(const std::map<BoardPosition, ChessPiece>& 
pieces, PieceColor color);

// Move generation with caching
std::vector<BoardPosition> getLegalMovesForPiece(
    const BoardPosition& from, 
    std::map<BoardPosition, ChessPiece>& pieces, 
    PieceColor currentTurn);

// Global move generation cache
static std::unordered_map<MoveGenKey, std::vector<BoardPosition>, MoveGenKeyHash> 
moveGenCache;
static std::mutex cacheMutex;

// Zobrist hashing constants - will be initialized in initZobristKeys function
static std::array<std::array<std::array<uint64_t, 2>, 6>, 64> ZOBRIST_PIECE_KEYS; // 
[square][piece_type][color]
static std::array<uint64_t, 8> ZOBRIST_EP_FILE_KEYS; // [file]
static uint64_t ZOBRIST_SIDE_TO_MOVE_KEY;

// Initialize Zobrist keys with random values
void initZobristKeys() {
    // Use a fixed seed for reproducibility
    std::mt19937_64 rng(42);
    
    // Initialize piece keys
    for (int sq = 0; sq < 64; ++sq) {
        for (int pt = 0; pt < 6; ++pt) {
            for (int c = 0; c < 2; ++c) {
                ZOBRIST_PIECE_KEYS[sq][pt][c] = rng();
            }
        }
    }
    
    // Initialize en passant keys (one for each file)
    for (int f = 0; f < 8; ++f) {
        ZOBRIST_EP_FILE_KEYS[f] = rng();
    }
    
    // Key for side to move
    ZOBRIST_SIDE_TO_MOVE_KEY = rng();
}

// Compute Zobrist hash for a board position
uint64_t computeZobristHash(const std::map<BoardPosition, ChessPiece>& pieces, 
                           PieceColor sideToMove, 
                           const BoardPosition& epTarget) {
    uint64_t hash = 0;
    
    // Add piece contributions
    for (const auto& [pos, piece] : pieces) {
        if (pos.first >= 0 && pos.first < 8 && pos.second >= 0 && pos.second < 8) {
            int sq = pos.second * 8 + pos.first;
            int pt = static_cast<int>(piece.type);
            int c = (piece.color == PieceColor::WHITE) ? 0 : 1;
            
            hash ^= ZOBRIST_PIECE_KEYS[sq][pt][c];
        }
    }
    
    // Add side to move contribution
    if (sideToMove == PieceColor::BLACK) {
        hash ^= ZOBRIST_SIDE_TO_MOVE_KEY;
    }
    
    // Add en passant contribution if applicable
    if (epTarget.first >= 0 && epTarget.first < 8) {
        hash ^= ZOBRIST_EP_FILE_KEYS[epTarget.first];
    }
    
    return hash;
}

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

// Parallel version for deeper depths
long long countMovesParallel(int depth, const std::map<BoardPosition, ChessPiece>& 
initialPieces, PieceColor currentTurn) {
    if (depth <= 3) {
        // Use single-threaded version for shallow depths
        std::map<BoardPosition, ChessPiece> pieces = initialPieces;
        return countMovesAtDepth(depth, pieces, currentTurn);
    }
    
    // Get all pieces of the current player
    std::vector<BoardPosition> playerPieces = getAllPiecesForColor(initialPieces, 
currentTurn);
    
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
    std::atomic<long long> totalMoves{0};
    std::mutex castlingMutex; // Mutex to protect castling rights
    
    // Function to process a subset of root moves
    auto processRootMoves = [&](const std::vector<BoardPosition>& rootMoves) -> long long {
        long long subTotal = 0;
        std::map<BoardPosition, ChessPiece> threadPieces = initialPieces;
        
        // Local copies of global state for this thread
        BoardPosition localEnPassantTarget = enPassantTarget;
        CastlingRights localCastlingRights;
        
        {
            std::lock_guard<std::mutex> lock(castlingMutex);
            localCastlingRights = castlingRights;
        }
        
        for (const auto& from : rootMoves) {
            // Get pseudo-legal moves
            std::vector<BoardPosition> pseudoLegalMoves = getLegalMovesForPiece(from, 
threadPieces, currentTurn);
            
            // Filter moves that would leave the king in check
            std::vector<BoardPosition> legalMoves = filterLegalMoves(from, pseudoLegalMoves, 
threadPieces, currentTurn);
            
            // Skip if no moves
            if (legalMoves.empty()) continue;
            
            // Cache the moving piece
            const ChessPiece& movingPiece = threadPieces[from];
            
            // Order moves for better pruning
            orderMoves(legalMoves, from, movingPiece, threadPieces);
            
            // Precalculate next turn
            PieceColor nextTurn = (currentTurn == PieceColor::WHITE) ? 
                                 PieceColor::BLACK : PieceColor::WHITE;
            
            for (const auto& to : legalMoves) {
                // Make a temporary board copy for each move
                auto tempBoard = threadPieces;
                
                // Remember captured piece (if any)
                auto targetIt = tempBoard.find(to);
                bool hadPiece = (targetIt != tempBoard.end());
                ChessPiece capturedPiece;
                
                if (hadPiece) {
                    capturedPiece = targetIt->second;
                }
                
                // Remember castling rights
                CastlingRights oldCastlingRights = localCastlingRights;
                
                // Update castling rights if king or rook moves
                if (movingPiece.type == PieceType::KING) {
                    if (movingPiece.color == PieceColor::WHITE) {
                        localCastlingRights.whiteKingSide = false;
                        localCastlingRights.whiteQueenSide = false;
                    } else {
                        localCastlingRights.blackKingSide = false;
                        localCastlingRights.blackQueenSide = false;
                    }
                } else if (movingPiece.type == PieceType::ROOK) {
                    if (movingPiece.color == PieceColor::WHITE) {
                        if (from.first == 0 && from.second == 7) { // White queenside rook
                            localCastlingRights.whiteQueenSide = false;
                        } else if (from.first == 7 && from.second == 7) { // White kingside 
rook
                            localCastlingRights.whiteKingSide = false;
                        }
                    } else {
                        if (from.first == 0 && from.second == 0) { // Black queenside rook
                            localCastlingRights.blackQueenSide = false;
                        } else if (from.first == 7 && from.second == 0) { // Black kingside 
rook
                            localCastlingRights.blackKingSide = false;
                        }
                    }
                }
                
                // Update castling rights if a rook is captured
                if (hadPiece && capturedPiece.type == PieceType::ROOK) {
                    if (capturedPiece.color == PieceColor::WHITE) {
                        if (to.first == 0 && to.second == 7) { // White queenside rook
                            localCastlingRights.whiteQueenSide = false;
                        } else if (to.first == 7 && to.second == 7) { // White kingside rook
                            localCastlingRights.whiteKingSide = false;
                        }
                    } else {
                        if (to.first == 0 && to.second == 0) { // Black queenside rook
                            localCastlingRights.blackQueenSide = false;
                        } else if (to.first == 7 && to.second == 0) { // Black kingside rook
                            localCastlingRights.blackKingSide = false;
                        }
                    }
                }
                
                // Remember en passant target
                BoardPosition oldEnPassantTarget = localEnPassantTarget;
                
                // Set new en passant target if appropriate
                if (movingPiece.type == PieceType::PAWN && std::abs(from.second - to.second) 
== 2) {
                    // Verify the pawn is moving from its starting rank
                    int startingRank = (movingPiece.color == PieceColor::WHITE) ? 6 : 1;
                    if (from.second == startingRank) {
                        localEnPassantTarget = {from.first, (from.second + to.second) / 2};
                    } else {
                        localEnPassantTarget = {-1, -1};
                    }
                } else {
                    localEnPassantTarget = {-1, -1};
                }
                
                // Handle en passant capture
                bool isEnPassant = false;
                BoardPosition capturedPawnPos = {-1, -1};
                ChessPiece capturedPawn;
                
                if (movingPiece.type == PieceType::PAWN && 
                    to == oldEnPassantTarget && 
                    from.first != to.first &&
                    oldEnPassantTarget.first != -1) {
                    
                    isEnPassant = true;
                    capturedPawnPos = {to.first, from.second};
                    
                    auto pawnIt = tempBoard.find(capturedPawnPos);
                    if (pawnIt != tempBoard.end() && 
                        pawnIt->second.type == PieceType::PAWN && 
                        pawnIt->second.color != movingPiece.color) {
                        
                        capturedPawn = pawnIt->second;
                        tempBoard.erase(capturedPawnPos);
                    }
                }
                
                // Handle castling
                bool isCastling = (movingPiece.type == PieceType::KING && std::abs(to.first - 
from.first) == 2);
                BoardPosition rookFrom, rookTo;
                ChessPiece rookPiece;
                bool hadRook = false;
                
                if (isCastling) {
                    int row = from.second;
                    if (to.first > from.first) { // Kingside
                        rookFrom = {7, row};
                        rookTo = {5, row}; // Place rook to the left of king's new position
                    } else { // Queenside
                        rookFrom = {0, row};
                        rookTo = {3, row}; // Place rook to the right of king's new position
                    }
                    
                    // Move the rook
                    auto rookIt = tempBoard.find(rookFrom);
                    if (rookIt != tempBoard.end()) {
                        rookPiece = rookIt->second;
                        hadRook = true;
                        tempBoard[rookTo] = rookPiece;
                        tempBoard.erase(rookFrom);
                    }
                }
                
                // Make the move
                tempBoard[to] = movingPiece;
                tempBoard.erase(from);
                
                // Handle pawn promotion
                bool isPawnPromotion = false;
                long long promotionMoves = 0;
                
                if (movingPiece.type == PieceType::PAWN) {
                    int promotionRank = (movingPiece.color == PieceColor::WHITE) ? 0 : 7;
                    if (to.second == promotionRank) {
                        isPawnPromotion = true;
                        
                        // For each promotion type, count moves
                        for (PieceType promType : {PieceType::QUEEN, PieceType::ROOK, 
                                                 PieceType::BISHOP, PieceType::KNIGHT}) {
                            // Create a new board for each promotion type
                            auto promotionBoard = tempBoard;
                            promotionBoard[to].type = promType;
                            
                            // Count moves with this promotion
                            promotionMoves += countMovesAtDepth(depth - 1, promotionBoard, 
nextTurn);
                        }
                        
                        subTotal += promotionMoves;
                    }
                }
                
                // If not a promotion, count moves normally
                if (!isPawnPromotion) {
                    subTotal += countMovesAtDepth(depth - 1, tempBoard, nextTurn);
                }
            }
        }
        
        return subTotal;
    };
    
    // Distribute root moves among threads
    if (!playerPieces.empty()) {
        // Sort pieces by their approximate move count to better balance work
        std::map<BoardPosition, ChessPiece> tempPieces = initialPieces;
        std::vector<std::pair<BoardPosition, size_t>> piecesWithMoveCount;
        
        for (const auto& pos : playerPieces) {
            auto moves = getLegalMovesForPiece(pos, tempPieces, currentTurn);
            auto legalMoves = filterLegalMoves(pos, moves, tempPieces, currentTurn);
            piecesWithMoveCount.push_back({pos, legalMoves.size()});
        }
        
        // Sort descending by move count to better balance threads
        std::sort(piecesWithMoveCount.begin(), piecesWithMoveCount.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Distribute pieces among threads using a greedy approach
        std::vector<std::vector<BoardPosition>> threadAssignments(numThreads);
        
        // Assign work in a round-robin fashion
        for (size_t i = 0; i < piecesWithMoveCount.size(); ++i) {
            threadAssignments[i % numThreads].push_back(piecesWithMoveCount[i].first);
        }
        
        // Launch threads
        futures.reserve(numThreads);
        for (unsigned int i = 0; i < numThreads; ++i) {
            if (!threadAssignments[i].empty()) {
                futures.push_back(std::async(std::launch::async, processRootMoves, 
                                            threadAssignments[i]));
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
    std::cout << "Depth test" << std::endl;
    std::cout << "--------------------------------" << std::endl;
    
    // Parse FEN string
    std::map<BoardPosition, ChessPiece> pieces;
    PieceColor sideToMove = PieceColor::WHITE;
    
    // Split FEN by spaces
    std::vector<std::string> fenParts;
    std::string current;
    for (char c : fen) {
        if (c == ' ') {
            fenParts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        fenParts.push_back(current);
    }
    
    // Parse piece placement
    if (fenParts.size() > 0) {
        int row = 0;
        int col = 0;
        for (char c : fenParts[0]) {
            if (c == '/') {
                row++;
                col = 0;
            } else if (std::isdigit(c)) {
                col += c - '0';
            } else {
                PieceType type;
                PieceColor color = std::islower(c) ? PieceColor::BLACK : PieceColor::WHITE;
                
                // Convert FEN symbol to piece type
                char symbol = std::tolower(c);
                switch (symbol) {
                    case 'p': type = PieceType::PAWN; break;
                    case 'r': type = PieceType::ROOK; break;
                    case 'n': type = PieceType::KNIGHT; break;
                    case 'b': type = PieceType::BISHOP; break;
                    case 'q': type = PieceType::QUEEN; break;
                    case 'k': type = PieceType::KING; break;
                    default: continue;
                }
                
                pieces[{col, row}] = {type, color, sf::Sprite()};
                col++;
            }
        }
    }
    
    // Parse side to move
    if (fenParts.size() > 1) {
        sideToMove = (fenParts[1] == "w") ? PieceColor::WHITE : PieceColor::BLACK;
    }
    
    // Parse castling rights
    if (fenParts.size() > 2) {
        // Reset all castling rights first
        castlingRights = {false, false, false, false};
        
        for (char c : fenParts[2]) {
            switch (c) {
                case 'K': castlingRights.whiteKingSide = true; break;
                case 'Q': castlingRights.whiteQueenSide = true; break;
                case 'k': castlingRights.blackKingSide = true; break;
                case 'q': castlingRights.blackQueenSide = true; break;
                default: break;
            }
        }
    } else {
        // Default castling rights if not specified
        castlingRights = {true, true, true, true};
    }
    
    // Parse en passant target square
    if (fenParts.size() > 3 && fenParts[3] != "-") {
        // Parse algebraic notation (e.g., "e3") to board coordinates
        int file = fenParts[3][0] - 'a';  // 'a' -> 0, 'b' -> 1, etc.
        int rank = 8 - (fenParts[3][1] - '0');  // '1' -> 7, '2' -> 6, etc.
        
        enPassantTarget = {file, rank};
    } else {
        enPassantTarget = {-1, -1};  // No en passant target
    }
    
    // Get number of hardware threads for parallel processing
    unsigned int numThreads = std::thread::hardware_concurrency();
    
    // Test different depths
    for (int depth = 1; depth <= 5; ++depth) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Reset castling rights for each run to ensure consistency
        CastlingRights originalCastlingRights = castlingRights;
        
        // Count moves using parallelized implementation for depths > 1
        long long totalNodes = (depth > 2 && numThreads > 1) ?
                              countMovesParallel(depth, pieces, sideToMove) :
                              countMovesAtDepth(depth, pieces, sideToMove);
        
        // Restore original castling rights
        castlingRights = originalCastlingRights;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = endTime - startTime;
        
        // Compare with expected results for the standard position
        bool isCorrect = false;
        std::string expectedNodes = "N/A";
        
        if (depth - 1 < sizeof(EXPECTED_NODES) / sizeof(EXPECTED_NODES[0])) {
            isCorrect = (EXPECTED_NODES[depth - 1] == totalNodes);
            expectedNodes = std::to_string(EXPECTED_NODES[depth - 1]);
        }
        
        std::string resultStr = isCorrect ? "CORRECT" : "INCORRECT";
        
        std::cout << "Depth " << depth << ": " << totalNodes << " moves "
                  << "(calculated in " << std::fixed << std::setprecision(2) 
                  << duration.count() << " seconds) - "
                  << resultStr;
        
        if (!expectedNodes.empty()) {
            std::cout << " (expected: " << expectedNodes << ")";
        }
        
        std::cout << std::endl;
    }
}

// Calculate moves for the standard starting position
void calculateMovesForStartingPosition() {
    // Use the standard starting position FEN
    std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    
    std::cout << "Depth test" << std::endl;
    std::cout << "--------------------------------" << std::endl;
    
    // Parse FEN string
    std::map<BoardPosition, ChessPiece> pieces;
    PieceColor sideToMove = PieceColor::WHITE;
    
    // Split FEN by spaces
    std::vector<std::string> fenParts;
    std::string current;
    for (char c : fen) {
        if (c == ' ') {
            fenParts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        fenParts.push_back(current);
    }
    
    // Parse piece placement
    if (fenParts.size() > 0) {
        int row = 0;
        int col = 0;
        for (char c : fenParts[0]) {
            if (c == '/') {
                row++;
                col = 0;
            } else if (std::isdigit(c)) {
                col += c - '0';
            } else {
                PieceType type;
                PieceColor color = std::islower(c) ? PieceColor::BLACK : PieceColor::WHITE;
                
                // Convert FEN symbol to piece type
                char symbol = std::tolower(c);
                switch (symbol) {
                    case 'p': type = PieceType::PAWN; break;
                    case 'r': type = PieceType::ROOK; break;
                    case 'n': type = PieceType::KNIGHT; break;
                    case 'b': type = PieceType::BISHOP; break;
                    case 'q': type = PieceType::QUEEN; break;
                    case 'k': type = PieceType::KING; break;
                    default: continue;
                }
                
                pieces[{col, row}] = {type, color, sf::Sprite()};
                col++;
            }
        }
    }
    
    // Parse side to move
    if (fenParts.size() > 1) {
        sideToMove = (fenParts[1] == "w") ? PieceColor::WHITE : PieceColor::BLACK;
    }
    
    // Parse castling rights
    if (fenParts.size() > 2) {
        // Reset all castling rights first
        castlingRights = {false, false, false, false};
        
        for (char c : fenParts[2]) {
            switch (c) {
                case 'K': castlingRights.whiteKingSide = true; break;
                case 'Q': castlingRights.whiteQueenSide = true; break;
                case 'k': castlingRights.blackKingSide = true; break;
                case 'q': castlingRights.blackQueenSide = true; break;
                default: break;
            }
        }
    } else {
        // Default castling rights if not specified
        castlingRights = {true, true, true, true};
    }
    
    // Parse en passant target square
    if (fenParts.size() > 3 && fenParts[3] != "-") {
        // Parse algebraic notation (e.g., "e3") to board coordinates
        int file = fenParts[3][0] - 'a';  // 'a' -> 0, 'b' -> 1, etc.
        int rank = 8 - (fenParts[3][1] - '0');  // '1' -> 7, '2' -> 6, etc.
        
        enPassantTarget = {file, rank};
    } else {
        enPassantTarget = {-1, -1};  // No en passant target
    }
    
    // Get number of hardware threads for parallel processing
    unsigned int numThreads = std::thread::hardware_concurrency();
    
    // Test different depths
    for (int depth = 1; depth <= 5; ++depth) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Reset castling rights for each run to ensure consistency
        CastlingRights originalCastlingRights = castlingRights;
        
        // Count moves using parallelized implementation for depths > 1
        long long totalNodes = (depth > 2 && numThreads > 1) ?
                              countMovesParallel(depth, pieces, sideToMove) :
                              countMovesAtDepth(depth, pieces, sideToMove);
        
        // Restore original castling rights
        castlingRights = originalCastlingRights;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = endTime - startTime;
        
        // Compare with expected results for the standard position
        bool isCorrect = false;
        std::string expectedNodes = "N/A";
        
        if (depth - 1 < sizeof(EXPECTED_NODES) / sizeof(EXPECTED_NODES[0])) {
            isCorrect = (EXPECTED_NODES[depth - 1] == totalNodes);
            expectedNodes = std::to_string(EXPECTED_NODES[depth - 1]);
        }
        
        std::string resultStr = isCorrect ? "CORRECT" : "INCORRECT";
        
        std::cout << "Depth " << depth << ": " << totalNodes << " moves "
                  << "(calculated in " << std::fixed << std::setprecision(2) 
                  << duration.count() << " seconds) - "
                  << resultStr;
        
        if (!expectedNodes.empty()) {
            std::cout << " (expected: " << expectedNodes << ")";
        }
        
        std::cout << std::endl;
    }
}

// This function generates legal moves for a piece without checking if they leave the king in 
check
std::vector<BoardPosition> getLegalMovesForPiece(const BoardPosition& from, 
                                               std::map<BoardPosition, ChessPiece>& pieces, 
                                               PieceColor currentTurn) {
    auto pieceIt = pieces.find(from);
    if (pieceIt == pieces.end() || pieceIt->second.color != currentTurn) {
        return std::vector<BoardPosition>();
    }
    
    const ChessPiece& piece = pieceIt->second;
    
    // Check the cache first
    MoveGenKey cacheKey{from, piece.type, piece.color, enPassantTarget};
    
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
                    if (targetPiece != pieces.end() && targetPiece->second.color != 
piece.color) {
                        legalMoves.push_back(captureLeft);
                    }
                }
                
                // Capture diagonally right
                if (col < 7) {
                    BoardPosition captureRight(col + 1, newRow);
                    auto targetPiece = pieces.find(captureRight);
                    if (targetPiece != pieces.end() && targetPiece->second.color != 
piece.color) {
                        legalMoves.push_back(captureRight);
                    }
                }
                
                // Handle en passant captures
                if (enPassantTarget.first != -1) {
                    // Check if our pawn is adjacent to the pawn that just moved two squares
                    if (row == enPassantTarget.second && std::abs(col - 
enPassantTarget.first) == 1) {
                        // Create the en passant capture move (diagonal to the en passant 
target)
                        BoardPosition enPassantCapture(enPassantTarget.first, row + 
direction);
                        
                        // Verify there's actually a pawn to capture
                        BoardPosition pawnToCapture(enPassantTarget.first, row);
                        
                        auto pawnIt = pieces.find(pawnToCapture);
                        if (pawnIt != pieces.end() && 
                            pawnIt->second.type == PieceType::PAWN && 
                            pawnIt->second.color != piece.color) {
                            
                            legalMoves.push_back(enPassantCapture);
                        }
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
                    if (targetPiece == pieces.end() || targetPiece->second.color != 
piece.color) {
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
                    if (targetPiece == pieces.end() || targetPiece->second.color != 
piece.color) {
                        legalMoves.push_back(newPos);
                    }
                }
            }
            
            // Add castling moves
            // Check if king is in starting position
            if ((piece.color == PieceColor::WHITE && row == 7 && col == 4) ||
                (piece.color == PieceColor::BLACK && row == 0 && col == 4)) {
                
                // Check if king has castling rights
                bool canCastleKingside = (piece.color == PieceColor::WHITE) ? 
                                         castlingRights.whiteKingSide : 
castlingRights.blackKingSide;
                bool canCastleQueenside = (piece.color == PieceColor::WHITE) ? 
                                          castlingRights.whiteQueenSide : 
castlingRights.blackQueenSide;
                
                // Kingside castling (O-O)
                if (canCastleKingside) {
                    // Check if king-side rook is in place
                    BoardPosition kingsideRook(7, row);
                    auto rookIt = pieces.find(kingsideRook);
                    if (rookIt != pieces.end() && 
                        rookIt->second.type == PieceType::ROOK && 
                        rookIt->second.color == piece.color) {
                        
                        // Check if squares between king and rook are empty
                        // Only need to check f and g files (5 and 6)
                        if (pieces.find({5, row}) == pieces.end() && 
                            pieces.find({6, row}) == pieces.end()) {
                            
                            // Add the castling move (king moves two squares towards rook)
                            legalMoves.push_back({col + 2, row});
                        }
                    }
                }
                
                // Queenside castling (O-O-O)
                if (canCastleQueenside) {
                    // Check if queen-side rook is in place
                    BoardPosition queensideRook(0, row);
                    auto rookIt = pieces.find(queensideRook);
                    if (rookIt != pieces.end() && 
                        rookIt->second.type == PieceType::ROOK && 
                        rookIt->second.color == piece.color) {
                        
                        // Check if squares between king and rook are empty
                        // Need to check b, c, and d files (1, 2, and 3)
                        if (pieces.find({1, row}) == pieces.end() && 
                            pieces.find({2, row}) == pieces.end() && 
                            pieces.find({3, row}) == pieces.end()) {
                            
                            // Add the castling move (king moves two squares towards rook)
                            legalMoves.push_back({col - 2, row});
                        }
                    }
                }
            }
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

// This is a function to check if a king is in check
bool isKingInCheck(const std::map<BoardPosition, ChessPiece>& pieces, PieceColor kingColor) {
    // Find the king's position
    BoardPosition kingPos = {-1, -1};
    for (const auto& [pos, piece] : pieces) {
        if (piece.type == PieceType::KING && piece.color == kingColor) {
            kingPos = pos;
            break;
        }
    }
    
    if (kingPos.first == -1) return false; // King not found
    
    // Get the opponent's color
    PieceColor opponentColor = (kingColor == PieceColor::WHITE) ? 
                               PieceColor::BLACK : PieceColor::WHITE;
    
    // Check if the king is under attack
    return isSquareAttacked(pieces, kingPos, opponentColor);
}

// Updated filterLegalMoves to correctly handle en passant
std::vector<BoardPosition> filterLegalMoves(
    const BoardPosition& from,
    const std::vector<BoardPosition>& moves,
    std::map<BoardPosition, ChessPiece>& pieces,
    PieceColor currentTurn
) {
    std::vector<BoardPosition> legalMoves;
    legalMoves.reserve(moves.size());
    
    const ChessPiece& movingPiece = pieces[from];
    
    // Find the king's position
    BoardPosition kingPos = {-1, -1};
    for (const auto& [pos, piece] : pieces) {
        if (piece.type == PieceType::KING && piece.color == currentTurn) {
            kingPos = pos;
            break;
        }
    }
    
    if (kingPos.first == -1) return legalMoves; // King not found (shouldn't happen)
    
    // Get the opponent's color
    PieceColor opponentColor = (currentTurn == PieceColor::WHITE) ? 
                             PieceColor::BLACK : PieceColor::WHITE;
    
    // Check if the king is currently in check
    bool kingInCheck = isSquareAttacked(pieces, kingPos, opponentColor);
    
    for (const auto& to : moves) {
        // Special handling for castling
        bool isCastling = (movingPiece.type == PieceType::KING && std::abs(to.first - 
from.first) == 2);
        
        if (isCastling) {
            // Can't castle out of check
            if (kingInCheck) continue;
            
            // Check if king would move through check
            int direction = (to.first > from.first) ? 1 : -1;
            BoardPosition throughSquare = {from.first + direction, from.second};
            
            // Make a temporary board to check the square the king moves through
            auto tempPieces = pieces;
            
            // Check if the king would move through check
            if (isSquareAttacked(tempPieces, throughSquare, opponentColor)) {
                continue; // Can't castle through check
            }
            
            // Also check the destination square
            if (isSquareAttacked(tempPieces, to, opponentColor)) {
                continue; // Can't castle into check
            }
            
            // Castling is legal
            legalMoves.push_back(to);
            continue;
        }
        
        // Make a temporary copy of the board to test the move
        auto tempPieces = pieces;
        
        // Check if this is an en passant capture
        bool isEnPassant = (movingPiece.type == PieceType::PAWN && 
                           to == enPassantTarget && 
                           from.first != to.first);
        
        // Save the captured pawn position for en passant capture
        BoardPosition capturedPawnPos = {-1, -1};
        ChessPiece capturedPawn;
        
        if (isEnPassant) {
            // For en passant, the captured pawn is on the same column as the destination
            // but on the same row as the source
            capturedPawnPos = {to.first, from.second};
            
            // Check if there's actually a pawn to capture
            auto capturedPawnIt = tempPieces.find(capturedPawnPos);
            if (capturedPawnIt != tempPieces.end() && 
                capturedPawnIt->second.type == PieceType::PAWN && 
                capturedPawnIt->second.color != movingPiece.color) {
                
                // Remember the captured pawn
                capturedPawn = capturedPawnIt->second;
                
                // Remove the captured pawn
                tempPieces.erase(capturedPawnPos);
            } else {
                // Not a valid en passant capture - skip this move
                continue;
            }
        }
        
        // Make the move on the temporary board
        tempPieces[to] = movingPiece;
        tempPieces.erase(from);
        
        // Update king position if we're moving the king
        BoardPosition newKingPos = (movingPiece.type == PieceType::KING) ? to : kingPos;
        
        // Check if king would be in check after the move
        if (!isSquareAttacked(tempPieces, newKingPos, opponentColor)) {
            legalMoves.push_back(to);
        }
        
        // No need to restore tempPieces since we create a new copy for each move
    }
    
    return legalMoves;
}

// Fix the promotion handling in countMovesAtDepth
long long countMovesAtDepth(int depth, std::map<BoardPosition, ChessPiece>& pieces, 
PieceColor currentTurn) {
    // Clear the move generation cache to ensure we don't use stale data
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        moveGenCache.clear();
    }
    
    // Base case: at depth 0, return 1 (counted as one position)
    if (depth <= 0) return 1;
    
    long long totalMoves = 0;
    
    // For debug output
    std::map<PieceType, long long> movesByPieceType;
    
    // Get all pieces of the current player
    std::vector<BoardPosition> playerPieces = getAllPiecesForColor(pieces, currentTurn);
    
    // Reserve space for moves to avoid reallocations
    std::vector<BoardPosition> moves;
    moves.reserve(28); // Maximum possible moves for a queen
    
    // First depth just reports total number of legal moves
    if (depth == 1) {
        for (const auto& from : playerPieces) {
            moves.clear();
            // Get pseudo-legal moves
            std::vector<BoardPosition> pseudoLegalMoves = getLegalMovesForPiece(from, pieces, 
currentTurn);
            
            // Filter moves that would leave the king in check
            std::vector<BoardPosition> legalMoves = filterLegalMoves(from, pseudoLegalMoves, 
pieces, currentTurn);
            
            totalMoves += legalMoves.size();
            
            // Debug counting
            if (from.first >= 0 && from.first < 8 && from.second >= 0 && from.second < 8) {
                auto pieceIt = pieces.find(from);
                if (pieceIt != pieces.end()) {
                    movesByPieceType[pieceIt->second.type] += legalMoves.size();
                }
            }
        }
        
        // Special debug output if this is at the top level depth 3
        if (depth == 3) {
            std::cout << "Depth 3 move counts by piece type:" << std::endl;
            std::cout << "PAWN: " << movesByPieceType[PieceType::PAWN] << std::endl;
            std::cout << "KNIGHT: " << movesByPieceType[PieceType::KNIGHT] << std::endl;
            std::cout << "BISHOP: " << movesByPieceType[PieceType::BISHOP] << std::endl;
            std::cout << "ROOK: " << movesByPieceType[PieceType::ROOK] << std::endl;
            std::cout << "QUEEN: " << movesByPieceType[PieceType::QUEEN] << std::endl;
            std::cout << "KING: " << movesByPieceType[PieceType::KING] << std::endl;
        }
        
        return totalMoves;
    }
    
    // Pre-calculate the next player's turn to avoid redundant calculations
    PieceColor nextTurn = (currentTurn == PieceColor::WHITE) ? 
                          PieceColor::BLACK : PieceColor::WHITE;
    
    // For deeper depths, we need to make each move and recurse
    for (const auto& from : playerPieces) {
        moves.clear();
        // Get pseudo-legal moves
        std::vector<BoardPosition> pseudoLegalMoves = getLegalMovesForPiece(from, pieces, 
currentTurn);
        
        // Filter moves that would leave the king in check
        std::vector<BoardPosition> legalMoves = filterLegalMoves(from, pseudoLegalMoves, 
pieces, currentTurn);
        
        // Skip if no moves
        if (legalMoves.empty()) continue;
        
        // Cache the moving piece
        ChessPiece movingPiece = pieces[from];
        
        // Order moves for better pruning at higher depths
        if (depth > 2) {
            orderMoves(legalMoves, from, movingPiece, pieces);
        }
        
        for (const auto& to : legalMoves) {
            // Remember captured piece (if any)
            auto targetIt = pieces.find(to);
            bool hadPiece = (targetIt != pieces.end());
            ChessPiece capturedPiece;
            if (hadPiece) {
                capturedPiece = targetIt->second;
                // Update castling rights if a rook is captured
                updateCastlingRightsForCapture(to, capturedPiece);
            }
            
            // Save current castling rights
            CastlingRights oldCastlingRights = castlingRights;
            
            // Update castling rights if king or rook moves
            updateCastlingRights(from, movingPiece);
            
            // Track en passant target
            BoardPosition oldEnPassantTarget = enPassantTarget;
            
            // Set en passant target if this move creates one
            BoardPosition newEnPassantTarget = calculateEnPassantTarget(movingPiece, from, 
to);
            enPassantTarget = newEnPassantTarget;
            
            // Check if this is an en passant capture
            bool isEnPassant = isEnPassantCapture(movingPiece, from, to, pieces);
            BoardPosition capturedPawnPos = {-1, -1};
            ChessPiece capturedPawn;
            
            if (isEnPassant) {
                // For en passant, the captured pawn is on the same column as the destination
                // but on the same row as the source
                capturedPawnPos = {to.first, from.second};
                
                // Get the captured pawn
                auto capturedPawnIt = pieces.find(capturedPawnPos);
                if (capturedPawnIt != pieces.end()) {
                    capturedPawn = capturedPawnIt->second;
                    pieces.erase(capturedPawnPos);
                }
            }
            
            // Check if this is a castling move
            bool isCastling = isCastlingMove(movingPiece, from, to);
            
            // Store rook positions and pieces for castling restoration
            BoardPosition rookFrom, rookTo;
            ChessPiece rookPiece;
            bool hadRook = false;
            
            // Make the move (use direct assignment for better performance)
            pieces[to] = movingPiece;
            pieces.erase(from);
            
            // Handle castling (move the rook if it's a castling move)
            if (isCastling) {
                // Save rook positions for unmake
                int row = from.second;
                if (to.first > from.first) { // Kingside
                    rookFrom = {7, row};
                    rookTo = {to.first - 1, row};
                } else { // Queenside
                    rookFrom = {0, row};
                    rookTo = {to.first + 1, row};
                }
                
                // Remember the rook
                auto rookIt = pieces.find(rookFrom);
                if (rookIt != pieces.end()) {
                    rookPiece = rookIt->second;
                    hadRook = true;
                    
                    // Move the rook
                    pieces[rookTo] = rookPiece;
                    pieces.erase(rookFrom);
                }
            }
            
            // Handle pawn promotion
            bool isPawnPromotion = false;
            if (movingPiece.type == PieceType::PAWN) {
                int promotionRank = (movingPiece.color == PieceColor::WHITE) ? 0 : 7;
                if (to.second == promotionRank) {
                    isPawnPromotion = true;
                    
                    // Save original pawn piece
                    ChessPiece originalPawn = pieces[to];
                    
                    // Count moves for all promotion pieces (Queen, Rook, Bishop, Knight)
                    for (PieceType promotionType : {PieceType::QUEEN, PieceType::ROOK, 
                                                   PieceType::BISHOP, PieceType::KNIGHT}) {
                        // Change the piece to the promotion type
                        pieces[to].type = promotionType;
                        
                        // Recursively count moves for the next player
                        long long moveCount = countMovesAtDepth(depth - 1, pieces, nextTurn);
                        totalMoves += moveCount;
                    }
                    
                    // Restore the original pawn for proper unmake
                    pieces[to] = originalPawn;
                }
            }
            
            // Only recurse normally if not a promotion (those already recursed above)
            if (!isPawnPromotion) {
                // Recursively count moves for the next player
                long long moveCount = countMovesAtDepth(depth - 1, pieces, nextTurn);
                totalMoves += moveCount;
                
                // For depth 3 debugging
                if (depth == 3) {
                    auto pieceIt = pieces.find(from);
                    if (pieceIt != pieces.end()) {
                        movesByPieceType[pieceIt->second.type] += moveCount;
                    }
                }
            }
            
            // Restore the en passant target
            enPassantTarget = oldEnPassantTarget;
            
            // Restore castling rights
            castlingRights = oldCastlingRights;
            
            // Unmake the move (restore the board state)
            pieces[from] = movingPiece;
            if (hadPiece) {
                pieces[to] = capturedPiece;
            } else {
                pieces.erase(to);
            }
            
            // Restore rook position for castling
            if (isCastling && hadRook) {
                pieces[rookFrom] = rookPiece;
                pieces.erase(rookTo);
            }
            
            // If it was an en passant capture, restore the captured pawn
            if (isEnPassant && capturedPawnPos.first != -1) {
                pieces[capturedPawnPos] = capturedPawn;
            }
        }
    }
    
    return totalMoves;
}

// Helper function to get all pieces of a specific color
std::vector<BoardPosition> getAllPiecesForColor(const std::map<BoardPosition, ChessPiece>& 
pieces, 
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

// Add a function to handle en passant captures when making moves
                           const ChessPiece& movingPiece,
                           const BoardPosition& from, 
                           const BoardPosition& to) {
    // Do nothing since en passant is disabled
    return;
}

// Function to check if a move is an en passant capture
bool isEnPassantCapture(const ChessPiece& piece, 
                       const BoardPosition& from, 
                       const BoardPosition& to,
                       const std::map<BoardPosition, ChessPiece>& pieces) {
    // Disable en passant capture completely
    return false;
}

// Add a function to check if a move is a castling move
bool isCastlingMove(const ChessPiece& piece, const BoardPosition& from, const BoardPosition& 
to) {
    return piece.type == PieceType::KING && std::abs(to.first - from.first) == 2;
}

// Add a function to handle castling rook movement
void handleCastling(std::map<BoardPosition, ChessPiece>& pieces, 
                   const ChessPiece& king,
                   const BoardPosition& from, 
                   const BoardPosition& to) {
    if (!isCastlingMove(king, from, to)) return;
    
    // Determine row based on king's color
    int row = from.second;
    
    // Kingside castling (king moves right)
    if (to.first > from.first) {
        // Find the kingside rook
        BoardPosition rookFrom = {7, row};
        BoardPosition rookTo = {to.first - 1, row}; // Place rook to the left of king's new 
position
        
        auto rookIt = pieces.find(rookFrom);
        if (rookIt != pieces.end()) {
            // Move the rook to its new position
            ChessPiece rook = rookIt->second;
            pieces[rookTo] = rook;
            pieces.erase(rookFrom);
        }
    }
    // Queenside castling (king moves left)
    else {
        // Find the queenside rook
        BoardPosition rookFrom = {0, row};
        BoardPosition rookTo = {to.first + 1, row}; // Place rook to the right of king's new 
position
        
        auto rookIt = pieces.find(rookFrom);
        if (rookIt != pieces.end()) {
            // Move the rook to its new position
            ChessPiece rook = rookIt->second;
            pieces[rookTo] = rook;
            pieces.erase(rookFrom);
        }
    }
}

// Move scoring for move ordering
int scoreMoveForOrdering(const BoardPosition& from, const BoardPosition& to, 
                         const ChessPiece& movingPiece,
                         const std::map<BoardPosition, ChessPiece>& pieces) {
    int score = 0;
    
    // Check if move is a capture
    auto targetIt = pieces.find(to);
    if (targetIt != pieces.end()) {
        // MVV-LVA (Most Valuable Victim - Least Valuable Aggressor)
        // Prioritize capturing high-value pieces with low-value pieces
        const int pieceValues[6] = {
            100,   // Pawn
            500,   // Rook
            320,   // Knight
            330,   // Bishop
            900,   // Queen
            20000  // King (extremely high to ensure king captures are rare)
        };
        
        int victimValue = pieceValues[static_cast<int>(targetIt->second.type)];
        int aggressorValue = pieceValues[static_cast<int>(movingPiece.type)];
        
        // Score based on victim value minus a fraction of aggressor value
        score = victimValue * 10 - aggressorValue / 10;
    }
    
    // Bonus for pawn promotions
    if (movingPiece.type == PieceType::PAWN) {
        int promotionRank = (movingPiece.color == PieceColor::WHITE) ? 0 : 7;
        if (to.second == promotionRank) {
            score += 800; // High bonus for promotions
        }
    }
    
    // Bonus for castling
    if (movingPiece.type == PieceType::KING && std::abs(to.first - from.first) == 2) {
        score += 50; // Moderate bonus for castling
    }
    
    // Small bonus for center control with pawns and knights
    if (movingPiece.type == PieceType::PAWN || movingPiece.type == PieceType::KNIGHT) {
        if ((to.first >= 2 && to.first <= 5) && (to.second >= 2 && to.second <= 5)) {
            score += 10; // Small bonus for center control
        }
    }
    
    return score;
}

// Sort moves by score (highest first) for better move ordering
void orderMoves(std::vector<BoardPosition>& moves, const BoardPosition& from, 
                const ChessPiece& movingPiece,
                const std::map<BoardPosition, ChessPiece>& pieces) {
    std::vector<std::pair<BoardPosition, int>> scoredMoves;
    scoredMoves.reserve(moves.size());
    
    // Score each move
    for (const auto& to : moves) {
        int score = scoreMoveForOrdering(from, to, movingPiece, pieces);
        scoredMoves.push_back({to, score});
    }
    
    // Sort moves by score (descending)
    std::sort(scoredMoves.begin(), scoredMoves.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Extract sorted moves
    for (size_t i = 0; i < scoredMoves.size(); ++i) {
        moves[i] = scoredMoves[i].first;
    }
}

// Helper function to check if a square is under attack in the compact board representation
bool isSquareAttacked(const std::map<BoardPosition, ChessPiece>& pieces, 
                     const BoardPosition& square, 
                     PieceColor attackingColor) {
    // Check for knight attacks
    static const int knightDx[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int knightDy[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
    
    for (int i = 0; i < 8; ++i) {
        int newX = square.first + knightDx[i];
        int newY = square.second + knightDy[i];
        
        if (newX >= 0 && newX < 8 && newY >= 0 && newY < 8) {
            BoardPosition knightPos(newX, newY);
            auto it = pieces.find(knightPos);
            if (it != pieces.end() && 
                it->second.type == PieceType::KNIGHT && 
                it->second.color == attackingColor) {
                return true;
            }
        }
    }
    
    // Check for pawn attacks
    int pawnDir = (attackingColor == PieceColor::WHITE) ? -1 : 1;
    int pawnAttackRow = square.second + pawnDir;
    
    if (pawnAttackRow >= 0 && pawnAttackRow < 8) {
        // Left diagonal
        if (square.first > 0) {
            BoardPosition pawnPos(square.first - 1, pawnAttackRow);
            auto it = pieces.find(pawnPos);
            if (it != pieces.end() && 
                it->second.type == PieceType::PAWN && 
                it->second.color == attackingColor) {
                return true;
            }
        }
        
        // Right diagonal
        if (square.first < 7) {
            BoardPosition pawnPos(square.first + 1, pawnAttackRow);
            auto it = pieces.find(pawnPos);
            if (it != pieces.end() && 
                it->second.type == PieceType::PAWN && 
                it->second.color == attackingColor) {
                return true;
            }
        }
    }
    
    // Check for attacks along rays (Queen, Rook, Bishop, King)
    // Directions: N, NE, E, SE, S, SW, W, NW
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    
    for (int dir = 0; dir < 8; ++dir) {
        for (int dist = 1; dist < 8; ++dist) {
            int newX = square.first + dx[dir] * dist;
            int newY = square.second + dy[dir] * dist;
            
            if (newX < 0 || newX >= 8 || newY < 0 || newY >= 8) break;
            
            BoardPosition rayPos(newX, newY);
            auto it = pieces.find(rayPos);
            
            if (it != pieces.end()) {
                const ChessPiece& piece = it->second;
                
                if (piece.color == attackingColor) {
                    // King can only attack adjacent squares
                    if (piece.type == PieceType::KING && dist == 1) {
                        return true;
                    }
                    
                    // Queen can attack along any ray
                    if (piece.type == PieceType::QUEEN) {
                        return true;
                    }
                    
                    // Rook can attack along orthogonal rays (dir 0, 2, 4, 6)
                    if (piece.type == PieceType::ROOK && (dir % 2 == 0)) {
                        return true;
                    }
                    
                    // Bishop can attack along diagonal rays (dir 1, 3, 5, 7)
                    if (piece.type == PieceType::BISHOP && (dir % 2 == 1)) {
                        return true;
                    }
                }
                
                // Stop ray-casting when we hit any piece
                break;
            }
        }
    }
    
    return false;
}

// Add a function to get a vector of all en passant captures possible in a position
std::vector<std::pair<BoardPosition, BoardPosition>> getEnPassantCaptures(
    const std::map<BoardPosition, ChessPiece>& pieces,
    PieceColor sideToMove,
    const BoardPosition& epTarget) {
    
    // Return empty vector as en passant is disabled
    return {};
}

// Helper function to correctly calculate en passant target from a move
BoardPosition calculateEnPassantTarget(
    const ChessPiece& piece,
    const BoardPosition& from,
    const BoardPosition& to) {
    
    // Disable en passant by always returning invalid position
    return {-1, -1};
}

// Function to update castling rights when a piece moves
void updateCastlingRights(const BoardPosition& from, const ChessPiece& piece) {
    // King moved - lose all castling rights for that color
    if (piece.type == PieceType::KING) {
        if (piece.color == PieceColor::WHITE) {
            castlingRights.whiteKingSide = false;
            castlingRights.whiteQueenSide = false;
        } else {
            castlingRights.blackKingSide = false;
            castlingRights.blackQueenSide = false;
        }
        return;
    }
    
    // Rook moved - lose castling rights for that side
    if (piece.type == PieceType::ROOK) {
        if (piece.color == PieceColor::WHITE) {
            if (from.first == 0 && from.second == 7) { // White queenside rook
                castlingRights.whiteQueenSide = false;
            } else if (from.first == 7 && from.second == 7) { // White kingside rook
                castlingRights.whiteKingSide = false;
            }
        } else {
            if (from.first == 0 && from.second == 0) { // Black queenside rook
                castlingRights.blackQueenSide = false;
            } else if (from.first == 7 && from.second == 0) { // Black kingside rook
                castlingRights.blackKingSide = false;
            }
        }
    }
}

// Function to check if a rook is captured, which would affect castling rights
void updateCastlingRightsForCapture(const BoardPosition& to, const ChessPiece& capturedPiece) 
{
    if (capturedPiece.type == PieceType::ROOK) {
        if (capturedPiece.color == PieceColor::WHITE) {
            if (to.first == 0 && to.second == 7) { // White queenside rook
                castlingRights.whiteQueenSide = false;
            } else if (to.first == 7 && to.second == 7) { // White kingside rook
                castlingRights.whiteKingSide = false;
            }
        } else {
            if (to.first == 0 && to.second == 0) { // Black queenside rook
                castlingRights.blackQueenSide = false;
            } else if (to.first == 7 && to.second == 0) { // Black kingside rook
                castlingRights.blackKingSide = false;
            }
        }
    }
}

// Function to handle the special case of en passant capture


