#include <SFML/Graphics.hpp> // Include SFML graphics header
#include <iostream>
#include "pieces_placement.h" // Include the pieces placement header

// Constants for chess board configuration
const unsigned int BOARD_SIZE = 8;
const float SQUARE_SIZE = 100.0f;
const unsigned int WINDOW_SIZE = BOARD_SIZE * SQUARE_SIZE;
const sf::Color LIGHT_SQUARE(222, 184, 135); // Light wood color
const sf::Color DARK_SQUARE(139, 69, 19);    // Dark wood color
const sf::Color BACKGROUND(50, 50, 50);      // Window background color
const float PIECE_SCALE_FACTOR = 1.1f;       // Make pieces 10% bigger

// Standard starting position in FEN notation
const std::string INITIAL_POSITION_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

void createChessGrid() {
    // Ask user for input - simplified message
    std::cout << "Starting Chess Board Application...\n"
              << "This is a chess board with pieces in starting position (FEN notation)\n"
              << "Type 1 to open the chess board window: ";
    
    std::string input;
    std::cin >> input;
    
    // Only proceed if user entered "1"
    if (input != "1") {
        std::cout << "Chess board not displayed." << std::endl;
        return;
    }
    
    // Load chess piece textures
    if (!loadPieceTextures(PIECE_SCALE_FACTOR)) {
        std::cout << "Warning: Some chess pieces could not be loaded.\n"
                  << "Make sure the 'pieces' folder exists with all piece images.\n"
                  << "Required naming format: white-rook.png, black-knight.png, etc." << std::endl;
    }
    
    // Initialize SFML window with proper settings
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8; // Enable antialiasing for smoother rendering
    
    sf::RenderWindow window(
        sf::VideoMode(WINDOW_SIZE, WINDOW_SIZE),
        "Chess Board",
        sf::Style::Titlebar | sf::Style::Close,
        settings
    );
    
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60); // Limit framerate to reduce CPU usage
    
    // Create and render the chess board texture (only once, for efficiency)
    sf::RenderTexture boardTexture;
    boardTexture.create(WINDOW_SIZE, WINDOW_SIZE);
    boardTexture.clear(BACKGROUND);
    
    // Draw the chess board pattern
    sf::RectangleShape square(sf::Vector2f(SQUARE_SIZE, SQUARE_SIZE));
    for (unsigned int i = 0; i < BOARD_SIZE; ++i) {
        for (unsigned int j = 0; j < BOARD_SIZE; ++j) {
            square.setPosition(i * SQUARE_SIZE, j * SQUARE_SIZE);
            square.setFillColor((i + j) % 2 == 0 ? LIGHT_SQUARE : DARK_SQUARE);
            boardTexture.draw(square);
        }
    }
    boardTexture.display();
    
    // Create a sprite from the board texture
    sf::Sprite boardSprite(boardTexture.getTexture());
    
    // Setup chess pieces using FEN notation
    std::map<std::pair<int, int>, ChessPiece> pieces;
    setupPositionFromFEN(pieces, INITIAL_POSITION_FEN);
    
    // Main loop
    while (window.isOpen()) {
        // Process events
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
                window.close();
        }

        // Only redraw when needed (no animations or changes in this version)
        window.clear(BACKGROUND);
        window.draw(boardSprite);
        drawPieces(window, pieces);
        window.display();
    }
}

