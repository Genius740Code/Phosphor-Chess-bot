#ifndef PIECES_MOVMENT_H
#define PIECES_MOVMENT_H

#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <utility>
#include <memory>
#include "pieces_placement.h"

/**
 * @brief Coordinates on the chess board
 */
using BoardPosition = std::pair<int, int>;

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
    
    // Selection state
    BoardPosition selectedSquare{-1, -1}; // (-1, -1) means no selection
    std::vector<BoardPosition> legalMoves; // Stores legal moves for selected piece
    
    // En passant tracking
    BoardPosition enPassantTarget{-1, -1}; // Position where en passant capture is possible
    
    // Promotion state
    bool awaitingPromotion = false;
    BoardPosition promotionSquare{-1, -1};
    PieceColor promotionColor;
    sf::RectangleShape promotionPanel;
    sf::RectangleShape promotionHeader;
    sf::RectangleShape promotionBorder;
    std::vector<sf::RectangleShape> promotionSelectionHighlights;
    std::vector<ChessPiece> promotionOptions;
    
    // Animation values
    float selectionAlpha = 180.0f;
    float selectionPulseDir = -1.0f;
    const float PULSE_SPEED = 120.0f;
    const float MIN_ALPHA = 120.0f;
    const float MAX_ALPHA = 200.0f;
    
    // Colors
    const sf::Color baseSelectionColor{173, 216, 230}; // Light blue
    sf::Color currentSelectionColor{173, 216, 230, 180}; // With alpha
    const sf::Color legalMoveColor{0, 200, 0, 130}; // Green with transparency
    const sf::Color promotionPanelColor{245, 245, 245, 240}; // Light panel with transparency
    const sf::Color promotionBorderColor{70, 70, 70, 255}; // Dark border
    const sf::Color promotionHeaderColor{30, 30, 30, 240}; // Header background
    
    // Board dimensions
    float squareSize;
    
    // Track if kings and rooks have moved (for castling)
    bool whiteKingMoved = false;
    bool blackKingMoved = false;
    bool whiteKingsideRookMoved = false;
    bool whiteQueensideRookMoved = false;
    bool blackKingsideRookMoved = false;
    bool blackQueensideRookMoved = false;
    
    // Private methods
    void calculateLegalMoves();
    std::vector<BoardPosition> getPawnMoves(const BoardPosition& pos, const ChessPiece& piece);
    std::vector<BoardPosition> getRookMoves(const BoardPosition& pos, const ChessPiece& piece);
    std::vector<BoardPosition> getKnightMoves(const BoardPosition& pos, const ChessPiece& piece);
    std::vector<BoardPosition> getBishopMoves(const BoardPosition& pos, const ChessPiece& piece);
    std::vector<BoardPosition> getQueenMoves(const BoardPosition& pos, const ChessPiece& piece);
    
    // Helper method for sliding pieces (rook, bishop, queen)
    void addMovesInDirection(std::vector<BoardPosition>& moves, const BoardPosition& startPos, 
                             int dirX, int dirY, const ChessPiece& piece);
                             
    // Promotion methods
    void showPromotionOptions(const BoardPosition& square, PieceColor color);
    void executePromotion(PieceType promotionType);
    
    // Executes a move with special rule handling
    void movePiece(const BoardPosition& from, const BoardPosition& to);
    bool isPromotionMove(const BoardPosition& from, const BoardPosition& to) const;
    bool isEnPassantMove(const BoardPosition& from, const BoardPosition& to) const;
    
    // Add King moves method
    std::vector<BoardPosition> getKingMoves(const BoardPosition& pos, const ChessPiece& piece);
    
    // Helper for castling
    bool canCastle(const BoardPosition& kingPos, int direction, const ChessPiece& king);
    
    // Add check for castling move
    bool isCastlingMove(const BoardPosition& from, const BoardPosition& to) const;

public:
    /**
     * @brief Constructor
     * @param piecesRef Reference to the map of chess pieces
     * @param squareSz Size of each square on the chess board
     */
    ChessInteraction(std::map<BoardPosition, ChessPiece>& piecesRef, float squareSz);
    
    /**
     * @brief Handles mouse click events on the chess board
     * @param x X coordinate of the mouse click
     * @param y Y coordinate of the mouse click
     * @return True if the interaction state changed
     */
    bool handleMouseClick(int x, int y);
    
    /**
     * @brief Updates animations and visual effects
     * @param deltaTime Time elapsed since last update (in seconds)
     */
    void update(float deltaTime);
    
    /**
     * @brief Draws interaction visuals (highlights, legal moves, etc.)
     * @param window The render window to draw to
     */
    void draw(sf::RenderWindow& window);
    
    /**
     * @brief Gets the currently selected square
     * @return Board position representing the selected square, or (-1,-1) if none
     */
    const BoardPosition& getSelectedSquare() const { return selectedSquare; }
    
    /**
     * @brief Clears the current selection
     */
    void clearSelection() { 
        selectedSquare = {-1, -1}; 
        legalMoves.clear();
    }
};

#endif // PIECES_MOVMENT_H