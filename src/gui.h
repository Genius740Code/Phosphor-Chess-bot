#ifndef GUI_H
#define GUI_H

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <memory>
#include "pieces_placement.h"
#include "pieces_movment.h"
#include "game_logic.h"

// Color constants for UI
const sf::Color BUTTON_COLOR(60, 60, 90, 200);
const sf::Color BUTTON_HOVER_COLOR(80, 80, 110, 230);

// Forward declarations
class ChessGameLogic;
class ChessInteraction;

class ChessBoard {
private:
    // Constants
    static constexpr unsigned int BOARD_SIZE = 8;
    static constexpr float SQUARE_SIZE = 100.0f;
    static constexpr unsigned int WINDOW_WIDTH = BOARD_SIZE * SQUARE_SIZE;
    static constexpr unsigned int WINDOW_HEIGHT = BOARD_SIZE * SQUARE_SIZE;
    static constexpr float PIECE_SCALE_FACTOR = 1.1f;
    
    // Standard starting position in FEN notation
    static const std::string INITIAL_POSITION_FEN;
    
    // Colors
    const sf::Color LIGHT_SQUARE{222, 184, 135}; // Light wood color
    const sf::Color DARK_SQUARE{139, 69, 19};    // Dark wood color
    const sf::Color BACKGROUND{50, 50, 50};      // Window background color
    
    // Board state
    std::map<std::pair<int, int>, ChessPiece> pieces;
    std::string currentFEN;
    sf::RenderTexture boardTexture;
    sf::Sprite boardSprite;
    
    // Optimization - cached rendering of board and pieces
    sf::RenderTexture boardPiecesTexture;
    sf::Sprite boardPiecesSprite;
    bool redrawBoardPiecesTexture;
    
    // Performance tracking
    int frameCounter;
    float frameTimeAccumulator;
    
    // Game logic
    std::unique_ptr<ChessGameLogic> gameLogic;
    
    // Interaction handler
    std::unique_ptr<ChessInteraction> interaction;
    
    // Game control buttons
    sf::RectangleShape newGameButton;
    sf::RectangleShape resetButton;
    
    // Timing
    sf::Clock clock;
    float deltaTime;
    
    // Window
    sf::RenderWindow window;
    
    // Button states
    bool newGameHovered = false;
    bool resetHovered = false;
    
    // Current game state for display
    GameState currentDisplayState = GameState::ACTIVE;
    PieceColor currentDisplayTurn = PieceColor::WHITE;
    
    // Private methods
    void createBoardTexture();
    void drawUI();
    void setupButtons();
    void handleEvents();
    void update();
    void render();
    bool handleButtonClick(int x, int y);
    bool isPointInButton(int x, int y, const sf::RectangleShape& button);
    void updateButtonHoverStates(int mouseX, int mouseY);
    void initializeGameStateDisplay();
    void updateGameStateDisplay();

public:
    ChessBoard();
    ~ChessBoard() = default;
    
    bool initialize(float pieceScaleFactor = PIECE_SCALE_FACTOR);
    void setPosition(const std::string& fen);
    void run();
};

/**
 * @brief Starts the chess application with a main menu
 * 
 * This function displays a menu where the user can choose to
 * play the game or exit. It handles the entire application lifecycle.
 */
void startChessApplication();

/**
 * @brief Creates and displays a chess board with pieces
 * 
 * This function handles the creation of the chess board window,
 * drawing the board pattern, and placing chess pieces according 
 * to the standard starting position.
 */
void displayChessBoard();

#endif // GUI_H 