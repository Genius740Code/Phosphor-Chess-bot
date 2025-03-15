#ifndef GUI_H
#define GUI_H

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include "pieces_placement.h"

class ChessBoard {
private:
    // Constants
    static constexpr unsigned int BOARD_SIZE = 8;
    static constexpr float SQUARE_SIZE = 100.0f;
    static constexpr unsigned int WINDOW_SIZE = BOARD_SIZE * SQUARE_SIZE;
    
    // Colors
    const sf::Color LIGHT_SQUARE{222, 184, 135}; // Light wood color
    const sf::Color DARK_SQUARE{139, 69, 19};    // Dark wood color
    const sf::Color BACKGROUND{50, 50, 50};      // Window background color
    
    // Board state
    std::map<std::pair<int, int>, ChessPiece> pieces;
    std::string currentFEN;
    sf::RenderTexture boardTexture;
    sf::Sprite boardSprite;
    
    // Window
    sf::RenderWindow window;
    
    // Private methods
    void createBoardTexture();
    void handleEvents();
    void render();

public:
    ChessBoard();
    ~ChessBoard() = default;
    
    bool initialize(float pieceScaleFactor = 1.1f);
    void setPosition(const std::string& fen);
    void run();
};

/**
 * @brief Creates a menu and displays a chess board with pieces
 * 
 * This function presents a simple menu where the user can choose to 
 * display the chess board or exit. When selected, it handles the creation 
 * of the chess board window, drawing the board pattern, and placing 
 * chess pieces according to the standard starting position.
 */
void createChessGrid();

#endif // GUI_H 