#include <SFML/Graphics.hpp> // Include SFML graphics header
#include <iostream>      // Include iostream for input/output
#include "gui.h"         // Include the header for GUI functions

int main() {
    try {
        // Just call the function directly - no need for redundant messages
        // since createChessGrid already has its own information output
        createChessGrid();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred!" << std::endl;
        return 1;
    }
}
