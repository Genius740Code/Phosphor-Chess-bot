#include <iostream>
#include <string>
#include <limits>
#include "gui.h"
#include "search.h"

// Forward declaration of the new search functions
void calculateMovesForPosition(const std::string& fen);
void calculateMovesForStartingPosition();

int main() {
    std::cout << "Chess Application\n";
    std::cout << "================\n\n";
    
    int choice = 0;
    std::string fen;
    
    while (choice != 4) {
        std::cout << "Choose an option:\n";
        std::cout << "1. Open GUI\n";
        std::cout << "2. Calculate starting position (you can choose depth)\n";
        std::cout << "3. Calculate custom position (you can choose depth)\n";
        std::cout << "4. Exit\n";
        std::cout << "Enter your choice (1-4): ";
        std::cin >> choice;
        
        // Clear any remaining input in buffer
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        switch (choice) {
            case 1:
                // Start the chess application with main menu
                startChessApplication();
                break;
            case 2:
                calculateMovesForStartingPosition();
                break;
            case 3:
                std::cout << "Enter FEN position: ";
                std::getline(std::cin, fen);
                calculateMovesForPosition(fen);
                break;
            case 4:
                std::cout << "Exiting...\n";
                break;
            default:
                std::cout << "Invalid choice. Please try again.\n";
                break;
        }
        
        std::cout << "\n";
    }
    
    return 0;
}
