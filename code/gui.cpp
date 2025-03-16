#include <SFML/Graphics.hpp> 
#include <iostream>
#include "gui.h"
#include "pieces_placement.h"
#include "pieces_movment.h"

// Constants for chess board configuration
const unsigned int BOARD_SIZE = 8;
const float SQUARE_SIZE = 100.0f;
const unsigned int WINDOW_SIZE = BOARD_SIZE * SQUARE_SIZE;
const sf::Color LIGHT_SQUARE(222, 184, 135); // Light wood color
const sf::Color DARK_SQUARE(139, 69, 19);    // Dark wood color
const sf::Color BACKGROUND(50, 50, 50);      // Window background color
const float PIECE_SCALE_FACTOR = 1.1f;       

// Define the static string constant
const std::string ChessBoard::INITIAL_POSITION_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Implementation of the ChessBoard class
ChessBoard::ChessBoard() : 
    currentFEN(INITIAL_POSITION_FEN),
    deltaTime(0.0f),
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
    
    // Setup coordinate labels
    setupCoordinates();
    
    // Load chess piece textures
    if (!loadPieceTextures(pieceScaleFactor)) {
        std::cerr << "Warning: Some chess pieces could not be loaded.\n"
                  << "Make sure the 'pieces' folder exists with all piece images.\n"
                  << "Required naming format: white-rook.png, black-knight.png, etc." << std::endl;
        return false;
    }
    
    // Setup initial position
    setPosition(currentFEN);
    
    // Initialize interaction handler
    interaction = std::make_unique<ChessInteraction>(pieces, SQUARE_SIZE);
    
    // Start the clock
    clock.restart();
    
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

void ChessBoard::setupCoordinates() {
    coordinateTexts.clear();
    
    // Files (A-H) along the bottom
    for (int i = 0; i < 8; i++) {
        sf::Text fileText;
        fileText.setFont(font);
        fileText.setString(std::string(1, 'a' + i));
        fileText.setCharacterSize(COORDINATE_SIZE);
        
        // Position the text at the bottom of each square
        float posX = i * SQUARE_SIZE + SQUARE_SIZE - COORDINATE_MARGIN - fileText.getLocalBounds().width;
        float posY = WINDOW_SIZE - COORDINATE_MARGIN - COORDINATE_SIZE;
        fileText.setPosition(posX, posY);
        
        // Set color based on square color
        fileText.setFillColor((i + 7) % 2 == 0 ? COORDINATE_DARK : COORDINATE_LIGHT);
        
        coordinateTexts.push_back(fileText);
    }
    
    // Ranks (1-8) along the left side (in reverse order to match chess notation)
    for (int i = 0; i < 8; i++) {
        sf::Text rankText;
        rankText.setFont(font);
        rankText.setString(std::to_string(8 - i));
        rankText.setCharacterSize(COORDINATE_SIZE);
        
        // Position the text at the left of each square
        float posX = COORDINATE_MARGIN;
        float posY = i * SQUARE_SIZE + COORDINATE_MARGIN;
        rankText.setPosition(posX, posY);
        
        // Set color based on square color
        rankText.setFillColor((i) % 2 == 0 ? COORDINATE_DARK : COORDINATE_LIGHT);
        
        coordinateTexts.push_back(rankText);
    }
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
        else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            interaction->handleMouseClick(event.mouseButton.x, event.mouseButton.y);
        }
    }
}

void ChessBoard::update() {
    // Update delta time
    deltaTime = clock.restart().asSeconds();
    
    // Update interactions (animations, etc.)
    interaction->update(deltaTime);
}

void ChessBoard::render() {
    window.clear(BACKGROUND);
    
    // Draw the board
    window.draw(boardSprite);
    
    // Draw coordinate labels
    for (const auto& text : coordinateTexts) {
        window.draw(text);
    }
    
    // Draw interaction effects (highlights)
    interaction->draw(window);
    
    // Draw pieces
    drawPieces(window, pieces);
    
    window.display();
}

void ChessBoard::run() {
    while (window.isOpen()) {
        handleEvents();
        update();
        render();
    }
}

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
            if (board.initialize()) {
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