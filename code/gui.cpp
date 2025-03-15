#include <SFML/Graphics.hpp> // Include SFML graphics header
#include <iostream>
#include "gui.h"
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

// Implementation of the ChessBoard class
ChessBoard::ChessBoard() : 
    currentFEN(INITIAL_POSITION_FEN),
    window(sf::VideoMode(WINDOW_SIZE, WINDOW_SIZE), 
           "Chess Board", 
           sf::Style::Titlebar | sf::Style::Close)
{
}

bool ChessBoard::initialize(float pieceScaleFactor) {
    // Set up window
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);
    
    // Create board texture
    createBoardTexture();
    
    // Load chess piece textures
    if (!loadPieceTextures(pieceScaleFactor)) {
        std::cerr << "Warning: Some chess pieces could not be loaded.\n"
                  << "Make sure the 'pieces' folder exists with all piece images.\n"
                  << "Required naming format: white-rook.png, black-knight.png, etc." << std::endl;
        return false;
    }
    
    // Setup initial position
    setPosition(currentFEN);
    
    return true;
}

void ChessBoard::createBoardTexture() {
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
    boardSprite.setTexture(boardTexture.getTexture());
}

void ChessBoard::setPosition(const std::string& fen) {
    currentFEN = fen;
    setupPositionFromFEN(pieces, fen);
}

void ChessBoard::handleEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed)
            window.close();
        else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
            window.close();
    }
}

void ChessBoard::render() {
    window.clear(BACKGROUND);
    window.draw(boardSprite);
    
    // Draw pieces
    drawPieces(window, pieces);
    
    window.display();
}

void ChessBoard::run() {
    while (window.isOpen()) {
        handleEvents();
        render();
    }
}

// Replace createChessGrid with a simple menu-based function
void createChessGrid() {
    std::cout << "Chess Board Application Menu\n";
    std::cout << "===========================\n";
    std::cout << "1. Open Chess Board\n";
    std::cout << "0. Exit\n";
    std::cout << "Enter your choice: ";
    
    int choice;
    std::cin >> choice;
    
    switch (choice) {
        case 1: {
            std::cout << "Starting Chess Board Application...\n";
            
            ChessBoard board;
            if (board.initialize(PIECE_SCALE_FACTOR)) {
                board.run();
            } else {
                std::cout << "Failed to initialize chess board." << std::endl;
            }
            break;
        }
        case 0:
        default:
            std::cout << "Exiting application.\n";
            break;
    }
}

