#include <SFML/Graphics.hpp> 
#include <iostream>
#include "gui.h"
#include "pieces_placement.h"
#include "pieces_movment.h"
#include "game_logic.h"

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
    frameCounter(0),
    frameTimeAccumulator(0.0f),
    deltaTime(0.0f),
    redrawBoardPiecesTexture(true),
    newGameHovered(false),
    resetHovered(false),
    window(sf::VideoMode(WINDOW_SIZE, WINDOW_SIZE), 
           "Chess Board", 
           sf::Style::Titlebar | sf::Style::Close)
{
}

bool ChessBoard::initialize(float pieceScaleFactor) {
    // Set up window with vsync and anti-aliasing
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);
    
    // Create board texture and intermediate render textures for better performance
    createBoardTexture();
    
    // Setup game control buttons
    setupButtons();
    
    // Load chess piece textures
    if (!loadPieceTextures(pieceScaleFactor)) {
        std::cerr << "Warning: Some chess pieces could not be loaded.\n"
                  << "Make sure the 'pieces' folder exists with all piece images.\n"
                  << "Required naming format: white-rook.png, black-knight.png, etc." << std::endl;
        return false;
    }
    
    // Setup initial position
    setPosition(currentFEN);
    
    // Create game logic
    gameLogic = std::make_unique<ChessGameLogic>(pieces);
    
    // Initialize interaction handler
    interaction = std::make_unique<ChessInteraction>(pieces, *gameLogic, SQUARE_SIZE);
    
    // Create game state message font and text
    initializeGameStateDisplay();
    
    // Start the clock
    clock.restart();
    
    // Create a render texture for caching the board + pieces
    if (!boardPiecesTexture.create(WINDOW_SIZE, WINDOW_SIZE, settings)) {
        std::cerr << "Error: Could not create board pieces texture." << std::endl;
        return false;
    }
    
    // Create a sprite for the cached board + pieces
    boardPiecesSprite.setTexture(boardPiecesTexture.getTexture());
    
    // Force initial render of the board + pieces
    redrawBoardPiecesTexture = true;
    
    return true;
}

void ChessBoard::setupButtons() {
    // Button dimensions
    const float buttonWidth = 100.0f;
    const float buttonHeight = 40.0f;
    const float buttonMargin = 20.0f;
    
    // New Game button
    newGameButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    newGameButton.setPosition(buttonMargin, buttonMargin);
    newGameButton.setFillColor(BUTTON_COLOR);
    newGameButton.setOutlineColor(sf::Color::White);
    newGameButton.setOutlineThickness(2.0f);
    
    // Reset Game button
    resetButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    resetButton.setPosition(WINDOW_SIZE - buttonWidth - buttonMargin, buttonMargin);
    resetButton.setFillColor(BUTTON_COLOR);
    resetButton.setOutlineColor(sf::Color::White);
    resetButton.setOutlineThickness(2.0f);
}

bool ChessBoard::isPointInButton(int x, int y, const sf::RectangleShape& button) {
    return x >= button.getPosition().x && 
           x <= button.getPosition().x + button.getSize().x &&
           y >= button.getPosition().y && 
           y <= button.getPosition().y + button.getSize().y;
}

void ChessBoard::updateButtonHoverStates(int mouseX, int mouseY) {
    // Check hover state for new game button
    if (isPointInButton(mouseX, mouseY, newGameButton)) {
        newGameHovered = true;
        newGameButton.setFillColor(BUTTON_HOVER_COLOR);
    } else {
        newGameHovered = false;
        newGameButton.setFillColor(BUTTON_COLOR);
    }
    
    // Check hover state for reset button
    if (isPointInButton(mouseX, mouseY, resetButton)) {
        resetHovered = true;
        resetButton.setFillColor(BUTTON_HOVER_COLOR);
    } else {
        resetHovered = false;
        resetButton.setFillColor(BUTTON_COLOR);
    }
}

bool ChessBoard::handleButtonClick(int x, int y) {
    // Check if new game button was clicked
    if (isPointInButton(x, y, newGameButton)) {
        // Reset the game
        setPosition(INITIAL_POSITION_FEN);
        gameLogic = std::make_unique<ChessGameLogic>(pieces);
        interaction = std::make_unique<ChessInteraction>(pieces, *gameLogic, SQUARE_SIZE);
        redrawBoardPiecesTexture = true;
        return true;
    }
    
    // Check if reset button was clicked
    if (isPointInButton(x, y, resetButton)) {
        // Reset the current game
        gameLogic->resetGame();
        interaction = std::make_unique<ChessInteraction>(pieces, *gameLogic, SQUARE_SIZE);
        redrawBoardPiecesTexture = true;
        return true;
    }
    
    return false;
}

void ChessBoard::createBoardTexture() {
    // Create render texture for the board
    if (!boardTexture.create(WINDOW_SIZE, WINDOW_SIZE)) {
        std::cerr << "Error: Could not create board texture." << std::endl;
        return;
    }
    
    // Start drawing to the texture
    boardTexture.clear(BACKGROUND);
    
    // Draw the checkerboard pattern
    sf::RectangleShape square(sf::Vector2f(SQUARE_SIZE, SQUARE_SIZE));
    
    for (unsigned int row = 0; row < BOARD_SIZE; ++row) {
        for (unsigned int col = 0; col < BOARD_SIZE; ++col) {
            // Determine square color
            square.setFillColor(((row + col) % 2 == 0) ? LIGHT_SQUARE : DARK_SQUARE);
            
            // Position the square
            square.setPosition(col * SQUARE_SIZE, row * SQUARE_SIZE);
            
            // Draw the square
            boardTexture.draw(square);
        }
    }
    
    // Finish drawing
    boardTexture.display();
    
    // Set up the sprite
    boardSprite.setTexture(boardTexture.getTexture());
}

void ChessBoard::setPosition(const std::string& fen) {
    // Store the current FEN
    currentFEN = fen;
    
    // Clear the current pieces
    pieces.clear();
    
    // Set up new position
    setupPositionFromFEN(pieces, fen);
    
    // Flag the board pieces texture for redraw
    redrawBoardPiecesTexture = true;
}

void ChessBoard::handleEvents() {
    sf::Event event;
    bool boardChanged = false;
    
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
        } else if (event.type == sf::Event::MouseButtonPressed) {
            if (event.mouseButton.button == sf::Mouse::Left) {
                // Check if a button was clicked
                if (handleButtonClick(event.mouseButton.x, event.mouseButton.y)) {
                    continue; // Button was clicked, don't process board click
                }
                
                // Handle click on board
                if (gameLogic->getGameState() == GameState::CHECK) {
                    // When in check, ensure move is legal to get out of check
                    const auto selectedSquare = interaction->getSelectedSquare();
                    
                    // Handle the click to see if it resolves the check
                    if (interaction->handleMouseClick(event.mouseButton.x, event.mouseButton.y)) {
                        // The board has changed
                        boardChanged = true;
                        
                        // If the game is no longer in check, the move was legal
                        if (gameLogic->getGameState() != GameState::CHECK) {
                            continue;
                        } else {
                            // If still in check, need to find another move
                            continue;
                        }
                    }
                } else {
                    // Normal move handling for non-check situations
                    if (interaction->handleMouseClick(event.mouseButton.x, event.mouseButton.y)) {
                        // The board has changed
                        boardChanged = true;
                    }
                }
            }
        } else if (event.type == sf::Event::MouseMoved) {
            // Update button hover states
            updateButtonHoverStates(event.mouseMove.x, event.mouseMove.y);
        }
    }
    
    // If the board changed, flag for redraw
    if (boardChanged) {
        redrawBoardPiecesTexture = true;
    }
}

void ChessBoard::update() {
    // Update time
    deltaTime = clock.restart().asSeconds();
    
    // Update interaction animations
    interaction->update(deltaTime);
    
    // Update game state display
    updateGameStateDisplay();
}

void ChessBoard::render() {
    // Clear the window
    window.clear(BACKGROUND);
    
    // If the board or pieces changed, redraw the cached texture
    if (redrawBoardPiecesTexture) {
        boardPiecesTexture.clear(BACKGROUND);
        
        // Draw the board
        boardPiecesTexture.draw(boardSprite);
        
        // Draw pieces
        for (const auto& [pos, piece] : pieces) {
            boardPiecesTexture.draw(piece.sprite);
        }
        
        boardPiecesTexture.display();
        redrawBoardPiecesTexture = false;
    }
    
    // Draw the cached board + pieces
    window.draw(boardPiecesSprite);
    
    // Draw interaction elements (selection, legal moves, etc.)
    interaction->draw(window);
    
    // Draw UI elements
    drawUI();
    
    // Display the result
    window.display();
}

void ChessBoard::drawUI() {
    // No longer drawing game state box
    
    // Draw buttons
    window.draw(newGameButton);
    
    // Draw "New Game" icon
    sf::CircleShape newGameIcon(8);
    newGameIcon.setFillColor(sf::Color::White);
    newGameIcon.setPosition(newGameButton.getPosition().x + 20, 
                           newGameButton.getPosition().y + (newGameButton.getSize().y - 16) / 2);
    window.draw(newGameIcon);
    
    // Draw a simple "+" icon on the New Game button
    sf::RectangleShape horizontalLine(sf::Vector2f(16, 2));
    horizontalLine.setFillColor(sf::Color::White);
    horizontalLine.setPosition(newGameIcon.getPosition().x - 4, newGameIcon.getPosition().y + 7);
    window.draw(horizontalLine);
    
    sf::RectangleShape verticalLine(sf::Vector2f(2, 16));
    verticalLine.setFillColor(sf::Color::White);
    verticalLine.setPosition(newGameIcon.getPosition().x + 7, newGameIcon.getPosition().y - 4);
    window.draw(verticalLine);
    
    window.draw(resetButton);
    
    // Draw "Reset" icon - a circular arrow
    sf::CircleShape resetCircle(8, 12);  // Circle with 12 points to make it smoother
    resetCircle.setFillColor(sf::Color::Transparent);
    resetCircle.setOutlineColor(sf::Color::White);
    resetCircle.setOutlineThickness(2);
    resetCircle.setPosition(resetButton.getPosition().x + 20, 
                           resetButton.getPosition().y + (resetButton.getSize().y - 16) / 2);
    window.draw(resetCircle);
    
    // Add arrow tip
    sf::ConvexShape arrowhead;
    arrowhead.setPointCount(3);
    arrowhead.setPoint(0, sf::Vector2f(resetCircle.getPosition().x + 12, resetCircle.getPosition().y - 2));
    arrowhead.setPoint(1, sf::Vector2f(resetCircle.getPosition().x + 18, resetCircle.getPosition().y + 2));
    arrowhead.setPoint(2, sf::Vector2f(resetCircle.getPosition().x + 15, resetCircle.getPosition().y + 8));
    arrowhead.setFillColor(sf::Color::White);
    window.draw(arrowhead);
}

void ChessBoard::initializeGameStateDisplay() {
    // We're no longer displaying game state text
    // This function now does nothing since we're removing the status box
}

void ChessBoard::updateGameStateDisplay() {
    // Only track the state for console output
    GameState state = gameLogic->getGameState();
    
    // If state changed, output to console
    if (state != currentDisplayState) {
        currentDisplayState = state;
        currentDisplayTurn = gameLogic->getCurrentTurn();
        
        std::string message;
        switch (state) {
            case GameState::CHECKMATE:
                message = (currentDisplayTurn == PieceColor::WHITE) ? 
                    "BLACK WINS BY CHECKMATE!" : "WHITE WINS BY CHECKMATE!";
                std::cout << "\n" << message << "\n" << std::endl;
                break;
                
            case GameState::STALEMATE:
                message = "GAME DRAWN BY STALEMATE";
                std::cout << "\n" << message << "\n" << std::endl;
                break;
                
            case GameState::DRAW_FIFTY:
                message = "GAME DRAWN BY FIFTY-MOVE RULE";
                std::cout << "\n" << message << "\n" << std::endl;
                break;
                
            case GameState::DRAW_REPETITION:
                message = "GAME DRAWN BY THREEFOLD REPETITION";
                std::cout << "\n" << message << "\n" << std::endl;
                break;
                
            case GameState::DRAW_MATERIAL:
                message = "GAME DRAWN BY INSUFFICIENT MATERIAL";
                std::cout << "\n" << message << "\n" << std::endl;
                break;
                
            case GameState::DRAW_AGREEMENT:
                message = "GAME DRAWN BY AGREEMENT";
                std::cout << "\n" << message << "\n" << std::endl;
                break;
                
            default:
                break;
        }
    }
}

void ChessBoard::run() {
    while (window.isOpen()) {
        handleEvents();
        update();
        render();
        
        // Track frame rate
        frameCounter++;
        frameTimeAccumulator += deltaTime;
        if (frameTimeAccumulator >= 1.0f) {
            float fps = static_cast<float>(frameCounter) / frameTimeAccumulator;
            window.setTitle("Chess Board - FPS: " + std::to_string(static_cast<int>(fps)));
            frameCounter = 0;
            frameTimeAccumulator = 0.0f;
        }
    }
}

// Creates the chess UI and runs the game
void displayChessBoard() {
    ChessBoard board;
    
    if (board.initialize()) {
        board.run();
    } else {
        std::cerr << "Failed to initialize chess board." << std::endl;
    }
}

// Add the main menu implementation with simplified UI
void startChessApplication() {
    // Window dimensions
    const unsigned int MENU_WIDTH = 400;
    const unsigned int MENU_HEIGHT = 300;
    
    sf::RenderWindow window(sf::VideoMode(MENU_WIDTH, MENU_HEIGHT), "Chess Game Menu", 
                           sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    
    // Set up menu rectangles - simplified and larger buttons
    sf::RectangleShape playButton(sf::Vector2f(250, 60));
    playButton.setFillColor(sf::Color(60, 120, 60));
    playButton.setOutlineColor(sf::Color::White);
    playButton.setOutlineThickness(3.0f);
    playButton.setPosition(75, 100);
    
    sf::RectangleShape exitButton(sf::Vector2f(250, 60));
    exitButton.setFillColor(sf::Color(180, 60, 60));
    exitButton.setOutlineColor(sf::Color::White);
    exitButton.setOutlineThickness(3.0f);
    exitButton.setPosition(75, 190);
    
    // Game title shape
    sf::RectangleShape titleBar(sf::Vector2f(300, 50));
    titleBar.setFillColor(sf::Color(50, 50, 100));
    titleBar.setOutlineColor(sf::Color::White);
    titleBar.setOutlineThickness(2.0f);
    titleBar.setPosition(50, 30);
    
    // Create chess piece icons for the title
    sf::CircleShape kingShape(15);
    kingShape.setFillColor(sf::Color::White);
    kingShape.setPosition(100, 40);
    
    sf::CircleShape queenShape(15);
    queenShape.setFillColor(sf::Color::Black);
    queenShape.setOutlineColor(sf::Color::White);
    queenShape.setOutlineThickness(2.0f);
    queenShape.setPosition(250, 40);
    
    // Menu loop
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    // Check if play option was clicked
                    sf::FloatRect playBounds = playButton.getGlobalBounds();
                    if (playBounds.contains(event.mouseButton.x, event.mouseButton.y)) {
                        window.close();
                        displayChessBoard();
                        return;
                    }
                    
                    // Check if exit option was clicked
                    sf::FloatRect exitBounds = exitButton.getGlobalBounds();
                    if (exitBounds.contains(event.mouseButton.x, event.mouseButton.y)) {
                        window.close();
                        return;
                    }
                }
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Num1 || event.key.code == sf::Keyboard::Numpad1 ||
                    event.key.code == sf::Keyboard::P || event.key.code == sf::Keyboard::Return) {
                    window.close();
                    displayChessBoard();
                    return;
                } else if (event.key.code == sf::Keyboard::Num2 || event.key.code == sf::Keyboard::Numpad2 ||
                          event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::E) {
                    window.close();
                    return;
                }
            } else if (event.type == sf::Event::MouseMoved) {
                // Check for button hover
                sf::FloatRect playBounds = playButton.getGlobalBounds();
                if (playBounds.contains(event.mouseMove.x, event.mouseMove.y)) {
                    playButton.setFillColor(sf::Color(80, 180, 80));
                } else {
                    playButton.setFillColor(sf::Color(60, 120, 60));
                }
                
                sf::FloatRect exitBounds = exitButton.getGlobalBounds();
                if (exitBounds.contains(event.mouseMove.x, event.mouseMove.y)) {
                    exitButton.setFillColor(sf::Color(220, 80, 80));
                } else {
                    exitButton.setFillColor(sf::Color(180, 60, 60));
                }
            }
        }
        
        // Draw menu
        window.clear(sf::Color(30, 30, 50));
        
        // Draw title background
        window.draw(titleBar);
        
        // Draw chess icons
        window.draw(kingShape);
        window.draw(queenShape);
        
        // Create "CHESS GAME" text with shapes
        // Letter C
        sf::CircleShape cShape(12, 20);
        cShape.setFillColor(sf::Color::Transparent);
        cShape.setOutlineColor(sf::Color::White);
        cShape.setOutlineThickness(2);
        cShape.setPosition(140, 45);
        cShape.setRotation(180);
        window.draw(cShape);
        
        // Letter H
        sf::RectangleShape h1(sf::Vector2f(3, 24));
        h1.setFillColor(sf::Color::White);
        h1.setPosition(170, 42);
        window.draw(h1);
        
        sf::RectangleShape h2(sf::Vector2f(12, 3));
        h2.setFillColor(sf::Color::White);
        h2.setPosition(170, 52);
        window.draw(h2);
        
        sf::RectangleShape h3(sf::Vector2f(3, 24));
        h3.setFillColor(sf::Color::White);
        h3.setPosition(182, 42);
        window.draw(h3);
        
        // Letter S
        sf::CircleShape sTop(6, 20);
        sTop.setFillColor(sf::Color::Transparent);
        sTop.setOutlineColor(sf::Color::White);
        sTop.setOutlineThickness(2);
        sTop.setPosition(197, 42);
        sTop.setRotation(180);
        window.draw(sTop);
        
        sf::CircleShape sBottom(6, 20);
        sBottom.setFillColor(sf::Color::Transparent);
        sBottom.setOutlineColor(sf::Color::White);
        sBottom.setOutlineThickness(2);
        sBottom.setPosition(203, 59);
        window.draw(sBottom);
        
        // Draw buttons
        window.draw(playButton);
        window.draw(exitButton);
        
        // Draw "PLAY" text with large, clear indicator
        sf::CircleShape playIcon(12);
        playIcon.setFillColor(sf::Color(255, 255, 255, 200));
        playIcon.setPosition(100, 120);
        window.draw(playIcon);
        
        sf::ConvexShape playArrow;
        playArrow.setPointCount(3);
        playArrow.setPoint(0, sf::Vector2f(108, 114));
        playArrow.setPoint(1, sf::Vector2f(108, 138));
        playArrow.setPoint(2, sf::Vector2f(128, 126));
        playArrow.setFillColor(sf::Color(60, 120, 60));
        window.draw(playArrow);
        
        // Simple shapes for "PLAY" text
        sf::RectangleShape playText1(sf::Vector2f(20, 5));
        playText1.setFillColor(sf::Color::White);
        playText1.setPosition(150, 128);
        window.draw(playText1);
        
        sf::RectangleShape playText2(sf::Vector2f(5, 20));
        playText2.setFillColor(sf::Color::White);
        playText2.setPosition(150, 118);
        window.draw(playText2);
        
        // Simple shapes for "EXIT" text
        sf::RectangleShape exitText1(sf::Vector2f(20, 5));
        exitText1.setFillColor(sf::Color::White);
        exitText1.setPosition(150, 218);
        window.draw(exitText1);
        
        sf::RectangleShape exitText2(sf::Vector2f(20, 5));
        exitText2.setFillColor(sf::Color::White);
        exitText2.setPosition(150, 208);
        window.draw(exitText2);
        
        sf::RectangleShape exitText3(sf::Vector2f(20, 5));
        exitText3.setFillColor(sf::Color::White);
        exitText3.setPosition(150, 228);
        window.draw(exitText3);
        
        sf::RectangleShape exitIcon(sf::Vector2f(20, 5));
        exitIcon.setFillColor(sf::Color::White);
        exitIcon.setPosition(100, 218);
        exitIcon.setRotation(45);
        window.draw(exitIcon);
        
        sf::RectangleShape exitIcon2(sf::Vector2f(20, 5));
        exitIcon2.setFillColor(sf::Color::White);
        exitIcon2.setPosition(114, 218);
        exitIcon2.setRotation(-45);
        window.draw(exitIcon2);
        
        window.display();
    }
}