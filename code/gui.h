#ifndef GUI_H
#define GUI_H

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <memory>
#include "pieces_placement.h"
#include "pieces_movment.h"

class ChessBoard {
private:
    // Constants
    static constexpr unsigned int BOARD_SIZE = 8;
    static constexpr float SQUARE_SIZE = 100.0f;
    static constexpr unsigned int WINDOW_SIZE = BOARD_SIZE * SQUARE_SIZE;
    static constexpr float PIECE_SCALE_FACTOR = 1.1f;
    static constexpr float COORDINATE_MARGIN = 5.0f;
    static constexpr float COORDINATE_SIZE = 16.0f;
    
    // Standard starting position in FEN notation
    static const std::string INITIAL_POSITION_FEN;
    
    // Colors
    const sf::Color LIGHT_SQUARE{222, 184, 135}; // Light wood color
    const sf::Color DARK_SQUARE{139, 69, 19};    // Dark wood color
    const sf::Color BACKGROUND{50, 50, 50};      // Window background color
    const sf::Color COORDINATE_LIGHT{255, 255, 255, 200}; // Light coordinate color
    const sf::Color COORDINATE_DARK{0, 0, 0, 200};       // Dark coordinate color
    
    // Board state
    std::map<std::pair<int, int>, ChessPiece> pieces;
    std::string currentFEN;
    sf::RenderTexture boardTexture;
    sf::Sprite boardSprite;
    
    // Coordinates
    sf::Font font;
    std::vector<sf::Text> coordinateTexts;
    
    // Interaction handler
    std::unique_ptr<ChessInteraction> interaction;
    
    // Timing
    sf::Clock clock;
    float deltaTime;
    
    // Window
    sf::RenderWindow window;
    
    // Private methods
    void createBoardTexture();
    void setupCoordinates();
    void handleEvents();
    void update();
    void render();

public:
    ChessBoard();
    ~ChessBoard() = default;
    
    bool initialize(float pieceScaleFactor = PIECE_SCALE_FACTOR);
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